#include "Simulation.h"
#include <iomanip>
using namespace std;

extern bool read_elf(char* file_name);
extern unsigned int cadr;	//code
extern unsigned int dadr;	//data
extern unsigned int badr;	//bss
extern unsigned int sdadr;	//sdata
extern unsigned int csize;
extern unsigned int dsize;
extern unsigned int bsize;
extern unsigned int sdsize;	
extern unsigned int vadr;		//code
extern unsigned int dvadr;		//data
extern unsigned int sdvadr;
extern unsigned long long gp;
extern unsigned long long gb;
extern unsigned int madr;
extern unsigned int endPC;
extern unsigned int entry;
extern FILE *file;

//#define DEBUG
//#define STEP

//指令运行数
long long inst_num=0;
long long cycles = 0;

//系统调用退出指示
int exit_flag=0;

//quit
int quit = 0;

//debug mode
int debug = 0;

//hazard
int data_hazard = 0;
int control_hazard = 0;
int load_use = 0;
int data_bypass = 0;

int load_file = 0;
char file_name[256];
struct BS_Ctrl
{
	char F_Stall, F_Bubble, D_Stall, D_Bubble, E_Stall, E_Bubble, M_Bubble, W_Bubble;
} bs_ctrl; 

//加载代码段
//初始化PC
void load_memory()
{
	memset(memory, 0, sizeof(memory));

	fseek(file,cadr,SEEK_SET);
	fread(&memory[vadr],1,csize,file);
	fseek(file,dadr,SEEK_SET);
	fread(&memory[dvadr],1,dsize,file);
	fseek(file,badr,SEEK_SET);
	fread(&memory[gb],1,bsize,file);
	//memset(&memory[gb],0,bsize);
	fseek(file,sdadr,SEEK_SET);
	fread(&memory[sdvadr],1,sdsize,file);
	//printf("%d %d %x\n", gb, bsize, &memory[gb]);

	fclose(file);
}

void shell()
{
	char cmd[256];
	while(1)
	{
		printf("$ ");
		cin >> cmd;
		if(strcmp(cmd, "m") == 0)
		{
			int s, num;
			cin >> s >> num;
			printf("check memory from 0x%x to 0x%x\n", s, s+num);
			for(int i = 0; i < num; ++i)
			{
				if(i % 16 == 0)
					printf("%x: ", s+i);
				printf("%02x ", memory[s+i]);
				if(i % 16 == 15)
					printf("\n");
			}
			printf("\n");
		}
		else if(strcmp(cmd, "q") == 0)
		{
			quit = 1;
			break;
		}
		else if(strcmp(cmd, "l") == 0)
		{
			cin >> file_name;
			if(read_elf(file_name))
			{
				load_file = 1;
				printf("file loaded\n");
			}
		}
		else if(strcmp(cmd, "d") == 0)
		{
			debug = !debug;
			if(debug)
			{
				printf("step mode\n");
			}
			else
			{
				printf("step mode canceled\n");
			}
		}
		else if(strcmp(cmd, "r") == 0)
		{
			if(!load_file)
			{
				printf("No file loaded!\n");
				continue;
			}
			printf("the program begin to run\n");
			break;
		}
		else
		{
			printf("Invalid command!\n");	
		}
	}
}

void step(){
	char cmd[256];
	while(1)
	{
		printf(">>");
		cin >> cmd;
		if(strcmp(cmd, "m") == 0)
		{
			int s, num;
			cin >> s >> num;
			printf("check memory from 0x%x to 0x%x\n", s, s+num);
			for(int i = 0; i < num; ++i)
			{
				if(i % 16 == 0)
					printf("%x: ", s+i);
				printf("%02x ", memory[s+i]);
				if(i % 16 == 15)
					printf("\n");
			}
			printf("\n");
		}
		else if(strcmp(cmd, "r") == 0)
		{
			for(int i = 0; i < 32; ++i)
			{
				printf("reg[%d]: %x\n", i, reg[i]);
			}
		}
		else if(strcmp(cmd, "c") == 0)
		{
			printf("continue...\n");
			break;
		}
		else if(strcmp(cmd, "g") == 0)
		{
			debug = 0;
			break;
		}
		else if(strcmp(cmd, "k") == 0)
		{
			exit_flag = 1;
			printf("kill the program\n");
			break;
		}
		else
		{
			printf("Invalid command!\n");
		}
	}
	
}

int main(int argc, char** argv)
{
	if(argc > 1)
	{
		memset(memory, 0, sizeof(memory));
		if(read_elf(argv[1]))
		{
			load_file = 1;
			printf("file loaded\n");
		}
	}
	while(1)
	{

		shell();
		if(quit == 1)
			break;

		//解析elf文件
		//read_elf(argv[1]);
		
		//加载内存
		//memset(memory, 0, sizeof(memory));
		load_memory();
		memset(&bs_ctrl,0,sizeof(bs_ctrl));

		//设置入口地址
		PC=entry;
		
		memset(reg, 0, sizeof(reg));
		//设置全局数据段地址寄存器
		reg[3]=gp;
		
		reg[2]=MAX/2;//栈基址 （sp寄存器）

		reg[0] = 0;
		inst_num = 0;
		cycles = 0;
		data_hazard = 0;
		control_hazard = 0;
		data_bypass = 0;
		load_use = 0;

		simulate();
		cout <<"simulate over!"<<endl;
		cout << "instruction number:    " << inst_num << endl;
		cout << "cycles:                " << cycles << endl;
		cout << "CPI:                   " << (double)cycles/inst_num << endl;	
		cout << "data_hazard number:    " << data_hazard << endl;
		cout << "load-use number:       " << load_use << endl;
		cout << "data_bypass number:    " << data_bypass << endl;
		cout << "control_hazard number: " << control_hazard << endl;

		debug = 0;
		exit_flag = 0;
		quit = 0;
	}
	
	return 0;
}

void simulate()
{
	//结束PC的设置
	int end=endPC;
	memset(&IF_ID, 0, sizeof(IF_ID));
	memset(&IF_ID_old, 0, sizeof(IF_ID_old));
	memset(&ID_EX, 0, sizeof(ID_EX));
	memset(&ID_EX_old, 0, sizeof(ID_EX_old));
	memset(&EX_MEM, 0, sizeof(EX_MEM));
	memset(&EX_MEM_old, 0, sizeof(EX_MEM_old));
	memset(&MEM_WB, 0, sizeof(MEM_WB));
	memset(&MEM_WB_old, 0, sizeof(MEM_WB_old));
	while(PC!=end)
	{
		inst_num++;
		old_PC = PC;
		cycles++;
		memset(&bs_ctrl, 0, sizeof(bs_ctrl));
		//运行
		WB();
		MEM();
		EX();
		ID();
		IF();

#ifdef STEP
		cin.get();
#endif
		//更新中间寄存器
		//IF
		if(bs_ctrl.F_Stall)
		{
			PC = old_PC;
			inst_num--;
		}

		// IF_ID
		if(bs_ctrl.D_Bubble)
		{
			memset(&IF_ID, 0, sizeof(IF_ID));
		}
		else if(!bs_ctrl.D_Stall)
		{
			IF_ID = IF_ID_old;
		}

		// ID_EX
		if(bs_ctrl.E_Bubble)
		{
			memset(&ID_EX, 0, sizeof(ID_EX));
		}
		else if(!bs_ctrl.E_Stall)
		{
			ID_EX = ID_EX_old;
		}

		EX_MEM=EX_MEM_old;
		MEM_WB=MEM_WB_old;

		if(exit_flag==1)
			break;
		if(quit == 1)
			break;

        reg[0]=0;//一直为零

#ifdef DEBUG
		for(int i = 0; i < 20; ++i)
		{
			printf("reg[%d]: %d\n", i, reg[i]);
		}
		int* p = (int*)&memory[69120];
		for(int i = 0; i < 10; ++i)
			printf("%d ", *(p+i));
		printf("\n");
#endif
		if(debug)
			step();

    }
}

//取指令
void IF()
{
	//write IF_ID_old
	IF_ID_old.inst=*((unsigned int*)&memory[PC]);
	IF_ID_old.PC=PC;
#ifdef DEBUG
	printf("Fetch instruction at %x\n", PC);
#endif
	PC = PC + 4;
}

//译码
void ID()
{
	//Read IF_ID
	unsigned int inst=IF_ID.inst;
	int temp_PC = IF_ID.PC;

	//printf("%x\n", temp_PC);

#ifdef DEBUG
	printf("%x\n", temp_PC);
	//printf("%x\n", inst);
#endif

	unsigned long long imm=0;
	unsigned long long Reg_Rs1=0,Reg_Rs2=0;
	int dst_PC=0;

	char RegDst = 0,ALUop = 0,ALUSrc = 0;
	char Branch = 0,MemRead = 0,MemWrite = 0;
	char RegWrite = 0,MemtoReg = 0;

	rs1= getbit(inst, 19, 15);
	rs2= getbit(inst, 24, 20);
	rd = getbit(inst, 11, 7);
	unsigned int fuc3=getbit(inst, 14, 12);
	unsigned int fuc7=getbit(inst, 31, 25);
	unsigned int OP = getbit(inst, 6, 0);
	immi = getbit(inst, 31, 20);
	imms = (getbit(inst, 31, 25) << 5) | getbit(inst, 11, 7);
	immsb = (getbit(inst, 31, 31) << 12) | (getbit(inst, 30, 25) << 5) | (getbit(inst, 11, 8) << 1) | (getbit(inst, 7, 7) << 11);
	immu = getbit(inst, 31, 12) << 12;
	immuj = (getbit(inst, 31, 31) << 20) | (getbit(inst, 30, 21) << 1) | (getbit(inst, 20, 20) << 11) | (getbit(inst, 19, 12) << 12);
#ifdef DEBUG
	printf("rs1: %d\n", rs1);
	printf("rs2: %d\n", rs2);
	printf("rd: %d\n", rd);
	printf("fuc3: %d\n", fuc3);
	printf("fuc7: %d\n", fuc7);
	printf("OP: %d\n", OP);
	//printf("immi: %d\n", ext_signed(immi, 12));
	//printf("imms: %d\n", ext_signed(imms, 12));
	//printf("immsb: %d\n", ext_signed(immsb, 13));
	//printf("immu: %d\n", ext_signed(immu, 32));
	//printf("immuj: %d\n", ext_signed(immuj, 21));

#endif

	if(OP==OP_R)
	{
		RegDst=rd;
		ALUop=ALU_ADD;
		ALUSrc=SRC_RS2;
		Branch=0;
		MemRead=0;
		MemWrite=0;
		RegWrite=1;
		MemtoReg=0;
		if(fuc3==F3_ADD&&fuc7==F7_ADD)
		{
			ALUop = ALU_ADD;
		}
		else if(fuc3 == F3_MUL && fuc7 == F7_MUL)
		{
			ALUop = ALU_MUL;
		}
		else if(fuc3 == F3_SUB && fuc7 == F7_SUB)
		{
			ALUop = ALU_SUB;
		}
		else if(fuc3 == F3_SLL && fuc7 == F7_SLL)
		{
			ALUop = ALU_SLL;
		}
		else if(fuc3 == F3_MULH && fuc7 == F7_MULH)	//	[127:64]
		{
			ALUop = ALU_MUL;
		}
		else if(fuc3 == F3_SLT && fuc7 == F7_SLT)
		{
			ALUop = ALU_SUB;		
		}
		else if(fuc3 == F3_XOR && fuc7 == F7_XOR)
		{
			ALUop = ALU_XOR;
		}
		else if(fuc3 == F3_DIV && fuc7 == F7_DIV)
		{
			ALUop = ALU_DIV;
		}
		else if(fuc3 == F3_SRL && fuc7 == F7_SRL)
		{
			ALUop = ALU_SRL;
		}
		else if(fuc3 == F3_SRA && fuc7 == F7_SRA)
		{
			ALUop = ALU_SRA;
		}
		else if(fuc3 == F3_OR && fuc7 == F7_OR)
		{
			ALUop = ALU_OR;
		}
		else if(fuc3 == F3_REM && fuc7 == F7_REM)
		{
			ALUop = ALU_REM;
		}
		else if(fuc3 == F3_AND && fuc7 == F7_AND)
		{
			ALUop = ALU_AND;
		}
	}
	else if(OP==OP_WR)
	{
		RegDst=rd;
		ALUSrc=SRC_RS2;
		Branch=0;
		MemRead=0;
		MemWrite=0;
		RegWrite=1;
		MemtoReg=0;
		if(fuc3 == F3_MULW && fuc7 == F7_MULW)
		{
			ALUop = ALU_MUL32;
		}
		else if(fuc3 == F3_ADDW && fuc7 == F7_ADDW)
		{
			ALUop = ALU_ADD32;
		}
		else if(fuc3 == F3_SUBW && fuc7 == F7_SUBW)
		{
			ALUop = ALU_SUB32;
		}
		else if(fuc3 == F3_DIVW && fuc7 == F7_DIVW)
		{
			ALUop = ALU_DIV32;
		}
	}
	else if(OP==OP_LW)
	{
		RegDst=rd;
		ALUop=ALU_ADD;
		ALUSrc=SRC_IMM;
		Branch=0;
		MemRead=1;
		MemWrite=0;
		RegWrite=1;
		MemtoReg=1;
		imm = ext_signed(immi, 12);
		if(fuc3==F3_LB)
		{
			MemRead = 8;
		}
		else if(fuc3 == F3_LH)
		{
			MemRead = 16;
		}
		else if(fuc3 == F3_LW)
		{
			MemRead = 32;
		}
		else if(fuc3 == F3_LD)
		{
			MemRead = 64;
		}
	}
	else if(OP==OP_I)
	{
		RegDst=rd;
		ALUop=ALU_ADD;
		ALUSrc=SRC_IMM;
		Branch=0;
		MemRead=0;
		MemWrite=0;
		RegWrite=1;
		imm = ext_signed(immi, 12);
		MemtoReg = 0;
		if(fuc3==F3_ADDI)
		{
			ALUop = ALU_ADD;
		}
		else if(fuc3 == F3_SLLI && fuc7 == F7_SLLI)
		{
			ALUop = ALU_SLL;
		}
		else if(fuc3 == F3_SLTI)
		{
			ALUop = ALU_SUB;
		}
		else if(fuc3 == F3_XORI)
		{
			ALUop = ALU_XOR;
			imm = immi;
		}
		else if(fuc3 == F3_SRLI && fuc7 == F7_SRLI)
		{
			ALUop = ALU_SRL;
		}
		else if (fuc3 == F3_SRAI && fuc7 == F7_SRAI)
		{
			ALUop = ALU_SRA;
		}
		else if(fuc3 == F3_ORI)
		{
			ALUop = ALU_OR;
			imm = immi;
		}
		else if(fuc3 == F3_ANDI)
		{
			ALUop = ALU_AND;
			imm = immi;
		}
		else
		{
			printf("DE Invalid\n");
		}
	}
	else if(OP==OP_ADDIW)
	{
		RegDst=rd;
		ALUop=ALU_ADD;
		ALUSrc=SRC_IMM;
		Branch=0;
		MemRead=0;
		MemWrite=0;
		RegWrite=1;
		imm = ext_signed(immi, 12);
		MemtoReg=0;
		if(fuc3==F3_ADDIW)
		{
			ALUop = ALU_ADD32;
		}
		else if(fuc3 == F3_SLLIW)
		{
			ALUop = ALU_SLL32;
		}
		else if (fuc3 == F3_SRLIW)
		{
			ALUop = ALU_SRL32;
		}
		else
		{
			printf("Invalid\n");
		}
	}
	else if(OP==OP_JALR && fuc3 == F3_JALR)
	{
		RegDst=rd;
		ALUop=ALU_ADD;
		ALUSrc=SRC_PC_4;
		Branch=B_JALR;
		MemRead=0;
		MemWrite=0;
		RegWrite=1;
		imm = ext_signed(immi, 12);
		MemtoReg=0;
		dst_PC=(reg[rs1] + imm) & -2LL;
	}
	else if(OP==OP_ECALL)
	{
		RegDst=rd;
		ALUop=ALU_ADD;
		ALUSrc=SRC_IMM;
		Branch=0;
		MemRead=0;
		MemWrite=0;
		RegWrite=1;
		imm = ext_signed(immi, 12);
		MemtoReg=0;
	}
	else if(OP == OP_S)
	{
		RegDst=0;
		ALUop=ALU_ADD;
		ALUSrc=SRC_IMM;
		Branch=0;
		MemRead=0;
		MemWrite=1;
		RegWrite=0;
		imm = ext_signed(imms, 12);
		MemtoReg=0;
		if(fuc3 == F3_SB)
		{
			MemWrite = 8;
		}
		else if(fuc3 == F3_SH)
		{
			MemWrite = 16;
		}
		else if (fuc3 == F3_SW)
		{
			MemWrite = 32;
		}
		else if(fuc3 == F3_SD)
		{
			MemWrite = 64;
		}
		else
		{
			printf("Invalid\n");
		}
	}
	else if(OP == OP_SB)
	{
		RegDst=0;
		ALUop=ALU_SUB;
		ALUSrc=SRC_RS2;
		Branch=1;
		MemRead=0;
		MemWrite=0;
		RegWrite=0;
		imm = ext_signed(immsb, 13);
		MemtoReg=0;
		dst_PC = temp_PC + imm;
		if(fuc3 == F3_BEQ)
		{
			Branch = B_BEQ;
		}
		else if(fuc3 == F3_BNE)
		{
			Branch = B_BNE;
		}
		else if(fuc3 == F3_BLT)
		{
			Branch = B_BLT;
		}
		else if(fuc3 == F3_BGE)
		{
			Branch = B_BGE;
		}
		else
		{
			printf("Invalid\n");
		}
	}
	else if(OP == OP_AUIPC)
	{
		RegDst=rd;
		ALUop=ALU_ADD;
		ALUSrc=SRC_PC_IMM;
		Branch=0;
		MemRead=0;
		MemWrite=0;
		RegWrite=1;
		imm = ext_signed(immu, 32);
		MemtoReg=0;
	}
	else if(OP == OP_LUI)
	{
		RegDst=rd;
		ALUop=ALU_ADD;
		rs1 = 0;
		ALUSrc=SRC_IMM;
		Branch=0;
		MemRead=0;
		MemWrite=0;
		RegWrite=1;
		imm = ext_signed(immu, 32);
		MemtoReg=0;
	}
	else if(OP == OP_JAL)
	{
		RegDst=rd;
		ALUop=ALU_ADD;
		ALUSrc=SRC_PC_4;
		Branch=B_JAL;
		MemRead=0;
		MemWrite=0;
		RegWrite=1;
		imm = ext_signed(immuj, 21);
		MemtoReg=0;
		dst_PC = temp_PC + imm;
	}
	else
	{
		printf("Invalid\n");
	}

	//write ID_EX_old
	ID_EX_old.op = OP;
	ID_EX_old.fuc3 = fuc3;
	ID_EX_old.fuc7 = fuc7;
	ID_EX_old.inst=inst;
	ID_EX_old.Rd=rd;
	ID_EX_old.Rs1=rs1;
	ID_EX_old.Rs2=rs2;
	ID_EX_old.PC = temp_PC;
	ID_EX_old.Imm = imm;
	ID_EX_old.Reg_Rs1 = reg[rs1];
	ID_EX_old.Reg_Rs2 = reg[rs2];
	if(OP == OP_R || OP == OP_S || OP == OP_SB || OP == OP_WR ||
		OP == OP_I || OP == OP_LW || OP == OP_ADDIW || OP == OP_JALR)
	{
		if(rs1 == ID_EX.Rd && 
			(ID_EX.op == OP_R ||
				ID_EX.op == OP_I || 
				ID_EX.op == OP_LUI || 
				ID_EX.op == OP_AUIPC || 
				ID_EX.op == OP_JAL || 
				ID_EX.op == OP_JALR ||
				ID_EX.op == OP_ADDIW ||
				ID_EX.op == OP_WR))
		{
			data_hazard++;
			data_bypass++;
			ID_EX_old.Reg_Rs1 = EX_MEM_old.ALU_out;
		}
		else if(rs1 == ID_EX.Rd && ID_EX.op == OP_LW)
		{
			data_hazard++;
			load_use++;
			bs_ctrl.D_Stall = 1;
			bs_ctrl.F_Stall = 1;
			bs_ctrl.E_Bubble = 1;
		}
		else if(rs1 == EX_MEM.Reg_dst && 
				(EX_MEM.op == OP_R ||
					EX_MEM.op == OP_I || 
					EX_MEM.op == OP_ADDIW ||
					EX_MEM.op == OP_LUI || 
					EX_MEM.op == OP_AUIPC || 
					EX_MEM.op == OP_JAL || 
					EX_MEM.op == OP_JALR ||
					EX_MEM.op == OP_WR))
		{
			data_hazard++;
			data_bypass++;
			ID_EX_old.Reg_Rs1 = MEM_WB_old.ALU_out;
		}
		else if(rs1 == EX_MEM.Reg_dst && EX_MEM.op == OP_LW)
		{
			data_bypass++;
			ID_EX_old.Reg_Rs1 = MEM_WB_old.Mem_read;
		}
		else
		{
			ID_EX_old.Reg_Rs1 = reg[rs1];	
		}
	}
	
	if(OP == OP_R || OP == OP_S || OP == OP_SB || OP == OP_WR)
	{
		if(rs2 == ID_EX.Rd && 
			(ID_EX.op == OP_R ||
				ID_EX.op == OP_I || 
				ID_EX.op == OP_LUI || 
				ID_EX.op == OP_AUIPC || 
				ID_EX.op == OP_JAL || 
				ID_EX.op == OP_JALR ||
				ID_EX.op == OP_ADDIW ||
				ID_EX.op == OP_WR))
		{
			data_hazard++;
			data_bypass++;
			ID_EX_old.Reg_Rs2 = EX_MEM_old.ALU_out;
		}
		else if(rs2 == ID_EX.Rd && ID_EX.op == OP_LW)
		{
			data_hazard++;
			load_use++;
			bs_ctrl.D_Stall = 1;
			bs_ctrl.F_Stall = 1;
			bs_ctrl.E_Bubble = 1;
		}
		else if(rs2 == EX_MEM.Reg_dst && 
				(EX_MEM.op == OP_R ||
					EX_MEM.op == OP_I || 
					EX_MEM.op == OP_ADDIW ||
					EX_MEM.op == OP_LUI || 
					EX_MEM.op == OP_AUIPC || 
					EX_MEM.op == OP_JAL || 
					EX_MEM.op == OP_JALR ||
					EX_MEM.op == OP_WR))
		{
			data_hazard++;
			data_bypass++;
			ID_EX_old.Reg_Rs2 = MEM_WB_old.ALU_out;
		}
		else if(rs2 == EX_MEM.Reg_dst && EX_MEM.op == OP_LW)
		{
			data_bypass++;
			ID_EX_old.Reg_Rs2 = MEM_WB_old.Mem_read;
		}
		else
		{
			ID_EX_old.Reg_Rs2 = reg[rs2];	
		}
	}

	ID_EX_old.dst_PC = dst_PC;
	//...

	ID_EX_old.Ctrl_EX_ALUOp = ALUop;
	ID_EX_old.Ctrl_EX_ALUSrc = ALUSrc;
	ID_EX_old.Ctrl_EX_RegDst = RegDst;
	ID_EX_old.Ctrl_M_Branch = Branch;
	ID_EX_old.Ctrl_M_MemWrite = MemWrite;
	ID_EX_old.Ctrl_M_MemRead = MemRead;
	ID_EX_old.Ctrl_WB_RegWrite = RegWrite;
	ID_EX_old.Ctrl_WB_MemtoReg = MemtoReg;
	//....

}

//执行
void EX()
{	
	//read ID_EX
	unsigned int inst = ID_EX.inst;
	unsigned int op = ID_EX.op;
	unsigned int fuc3 = ID_EX.fuc3;
	unsigned int fuc7 = ID_EX.fuc7;
	int temp_PC=ID_EX.PC;
	int Rd = ID_EX.Rd;
	int Rs1 = ID_EX.Rs1;
	int Rs2 = ID_EX.Rs2;
	int dst_PC = ID_EX.dst_PC;
	unsigned long long imm = ID_EX.Imm;
	REG Reg_Rs1 = ID_EX.Reg_Rs1;
	REG Reg_Rs2 = ID_EX.Reg_Rs2;
	char RegDst = ID_EX.Ctrl_EX_RegDst;
	char ALUOp = ID_EX.Ctrl_EX_ALUOp;
	char ALUSrc = ID_EX.Ctrl_EX_ALUSrc;
	char Branch = ID_EX.Ctrl_M_Branch;

#ifdef DEBUG
	printf("EX: %x\n",temp_PC);
#endif

	EX_MEM_old.Ctrl_M_Branch = ID_EX.Ctrl_M_Branch;
	EX_MEM_old.Ctrl_M_MemWrite = ID_EX.Ctrl_M_MemWrite;
	EX_MEM_old.Ctrl_M_MemRead = ID_EX.Ctrl_M_MemRead;
	EX_MEM_old.Ctrl_WB_RegWrite = ID_EX.Ctrl_WB_RegWrite;
	EX_MEM_old.Ctrl_WB_MemtoReg = ID_EX.Ctrl_WB_MemtoReg;



	//choose ALU input number
	//...

	//alu calculate
	REG ALU_IN1, ALU_IN2;
	switch(ALUSrc)
	{
		case SRC_RS2:
			ALU_IN1 = Reg_Rs1;
			ALU_IN2 = Reg_Rs2;
			break;
		case SRC_IMM:
			ALU_IN1 = Reg_Rs1;
			ALU_IN2 = imm;
			break;
		case SRC_PC_IMM:
			ALU_IN1 = temp_PC;
			ALU_IN2 = imm;
			break;
		case SRC_PC_4:
			ALU_IN1 = temp_PC;
			ALU_IN2 = 4;
			break;
		default:break;
	}

	int Zero = 0;
	REG ALU_out = 0;
	switch(ALUOp){
		case ALU_ADD:
			ALU_out = ALU_IN1 + ALU_IN2;
			break;
		case ALU_SUB:
			ALU_out = ALU_IN1 - ALU_IN2;
			break;
		case ALU_MUL:
			cycles += 1;
			ALU_out = ALU_IN1 * ALU_IN2;
			break;
		case ALU_SLL:
			ALU_out = ALU_IN1 << ALU_IN2;
			break;
		case ALU_XOR:
			ALU_out = ALU_IN1 ^ ALU_IN2;
			break;
		case ALU_DIV:
			cycles += 39;
			ALU_out = ALU_IN1 / ALU_IN2;
			break;
		case ALU_SRL:
			ALU_out = ALU_IN1 >> ALU_IN2;
			break;
		case ALU_SRA:
			ALU_out = (long long)ALU_IN1 >> ALU_IN2;
			break;
		case ALU_OR:
			ALU_out = ALU_IN1 | ALU_IN2;
			break;
		case ALU_REM:
			ALU_out = ALU_IN1 % ALU_IN2;
			break;
		case ALU_AND:
			ALU_out = ALU_IN1 & ALU_IN2;
			break;
		case ALU_ADD32:
			ALU_out = ext_signed((unsigned long long)((int)ALU_IN1 + (int)ALU_IN2) & ((1LL << 32) - 1), 32);
			break;
		case ALU_MUL32:
			cycles += 1;
			ALU_out = ext_signed((unsigned long long)((int)ALU_IN1 * (int)ALU_IN2) & ((1LL << 32) - 1), 32);
			break;
		case ALU_SUB32:
			ALU_out = ext_signed((unsigned long long)((int)ALU_IN1 - (int)ALU_IN2) & ((1LL << 32) - 1), 32);
			break;
		case ALU_DIV32:
			cycles += 39;
			ALU_out = ext_signed((unsigned long long)((int)ALU_IN1 / (int)ALU_IN2) & ((1LL << 32) - 1), 32);
			break;
		case ALU_SLL32:
			ALU_out = ext_signed(((int)ALU_IN1 << ALU_IN2), 32);
			break;
		case ALU_SRL32:
			ALU_out = ext_signed(((int)ALU_IN1 >> ALU_IN2), 32);
			break;
		default:break;
	}

	if( (op == OP_R && 
		fuc3 == F3_SLT && 
		fuc7 == F7_SLT) || 
		(op == OP_I && 
		fuc3 == F3_SLTI))
	{
		if((long long)ALU_out < 0)
		{
			ALU_out = 1;
		}
		else
		{
			ALU_out = 0;
		}
	}

	//choose reg dst address
	int Reg_dst = 0;
	if(RegDst)
	{
		Reg_dst = Rd;
	}
	else
	{
		Reg_dst = 0;
	}

	//Branch PC calulate
	//...
#ifdef DEBUG
	printf("%d %x %d\n", Branch, dst_PC, ALU_out);
#endif

	switch(Branch)
	{
		case B_JALR:
		case B_JAL:
			control_hazard++;
			inst_num--;
			bs_ctrl.F_Stall = 0;
			bs_ctrl.D_Stall = 0;
			bs_ctrl.E_Bubble = 1;
			PC = dst_PC;
			break;
		case B_BEQ:
			if((long long)ALU_out == 0)
			{
				control_hazard++;
				inst_num--;
				PC = dst_PC;
				bs_ctrl.F_Stall = 0;
				bs_ctrl.D_Stall = 0;
				bs_ctrl.E_Bubble = 1;
			}
			break;
		case B_BNE:
			if((long long)ALU_out != 0)
			{
				control_hazard++;
				inst_num--;
				PC = dst_PC;
				bs_ctrl.F_Stall = 0;
				bs_ctrl.D_Stall = 0;
				bs_ctrl.E_Bubble = 1;
			}
			break;
		case B_BLT:
			if((long long)ALU_out < 0)
			{
				control_hazard++;
				inst_num--;
				PC = dst_PC;
				bs_ctrl.F_Stall = 0;
				bs_ctrl.D_Stall = 0;
				bs_ctrl.E_Bubble = 1;
			}
			break;
		case B_BGE:
			if((long long)ALU_out >= 0)
			{
				control_hazard++;
				inst_num--;
				PC = dst_PC;
				bs_ctrl.F_Stall = 0;
				bs_ctrl.D_Stall = 0;
				bs_ctrl.E_Bubble = 1;
			}
			break;
		default:break;
	}

	//write EX_MEM_old
	EX_MEM_old.op = op;
	EX_MEM_old.fuc3 = fuc3;
	EX_MEM_old.fuc7 = fuc7;
	EX_MEM_old.PC = temp_PC;
	EX_MEM_old.Reg_dst = Reg_dst;
	EX_MEM_old.ALU_out = ALU_out;
	EX_MEM_old.Zero = Zero;
	EX_MEM_old.Reg_Rs2 = Reg_Rs2;
    //.....
}

//访问存储器
void MEM()
{
	
	//read EX_MEM
	unsigned int op = EX_MEM.op;
	unsigned int fuc3 = EX_MEM.fuc3;
	unsigned int fuc7 = EX_MEM.fuc7;
	int temp_PC = EX_MEM.PC;
	int Reg_dst = EX_MEM.Reg_dst;
	REG ALU_out = EX_MEM.ALU_out;
	int Zero = EX_MEM.Zero;
	REG Reg_Rs2 = EX_MEM.Reg_Rs2;

#ifdef DEBUG
	printf("mem: %x\n", temp_PC);
#endif

	char Branch = EX_MEM.Ctrl_M_Branch;
	char MemWrite = EX_MEM.Ctrl_M_MemWrite;
	char MemRead = EX_MEM.Ctrl_M_MemRead;

	unsigned long long Mem_read = 0;

	MEM_WB_old.Ctrl_WB_RegWrite = EX_MEM.Ctrl_WB_RegWrite;
	MEM_WB_old.Ctrl_WB_MemtoReg = EX_MEM.Ctrl_WB_MemtoReg;

	//complete Branch instruction PC change

	//read / write memory
#ifdef DEBUG
	printf("%d %d\n", ALU_out, MemRead);
#endif

	if(MemWrite)
	{
		memcpy(&memory[ALU_out], &Reg_Rs2, MemWrite / 8);
	}
	if(MemRead)
	{
		//printf("%d %d %x %x\n", ALU_out, MemRead / 8, &memory[ALU_out], &Mem_read);
		memcpy(&Mem_read, &memory[ALU_out], MemRead / 8);
	}

	//write MEM_WB_old
	MEM_WB_old.op = op;
	MEM_WB_old.fuc3 = fuc3;
	MEM_WB_old.fuc7 = fuc7;
	MEM_WB_old.Mem_read = Mem_read;
	MEM_WB_old.ALU_out = ALU_out;
	MEM_WB_old.Reg_dst = Reg_dst;
	MEM_WB_old.PC = temp_PC;
}


//写回
void WB()
{

	//read MEM_WB
	int temp_PC = MEM_WB.PC;
	unsigned long long Mem_read = MEM_WB.Mem_read;
	unsigned int op = MEM_WB.op;
	unsigned int fuc3 = MEM_WB.fuc3;
	unsigned int fuc7 = MEM_WB.fuc7;
	REG ALU_out = MEM_WB.ALU_out;
	int Reg_dst = MEM_WB.Reg_dst;

#ifdef DEBUG
	printf("wb: %x\nrd: %d\n", temp_PC, Reg_dst);
#endif

	char RegWrite = MEM_WB.Ctrl_WB_RegWrite;
	char MemtoReg = MEM_WB.Ctrl_WB_MemtoReg;

	//write reg
	if(RegWrite && Reg_dst != 0)
	{
		if(MemtoReg)
		{
			reg[Reg_dst] = Mem_read;
		}
		else
		{
			reg[Reg_dst] = ALU_out;
		}
	}
}