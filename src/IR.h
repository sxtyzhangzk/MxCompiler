#ifndef MX_COMPILER_IR_H
#define MX_COMPILER_IR_H

#include "common.h"
#include <cstdint>
#include <list>
#include <memory>
#include <vector>
#include <set>
#include <queue>
#include <functional>

namespace MxIR
{
	enum Operation
	{
		Nop,
		Add, Sub,			//dst = src1 op src2, dst & src1 & src2 must have the same size
		Mult, Div, Mod, 
		Shl, Shr,
		And, Or, Xor,
		Neg, Not,
		Load, Store,		//dst = load [src1]; store dst [src1] (src1 can be reg / imm / funcID / stringID / globalVarID / externalSymbolName)
		LoadA, StoreA,		//dst = load [src1 + src2 (signed)]; store dst [src1 + src2] 
		Move,				//dst = src, data moving from a small register to a large register is NOT allowed, use Sext/Zext instead
		Sext, Zext,			//sign extend & zero extend
		Slt, Sle, Seq, Sge, Sgt, Sne,	//dst = src1 op src2 ? true : false
		Sltu, Sleu, Sgeu, Sgtu,
		//Blt, Ble, Beq, Bge, Bgt, Bne,	//branch if src1 op src2
		Br,					//branch to brTrue if src1 == true (likely when src2 == 1, unlikely when src2 == 2)
		Jump,				//jump to brTrue
		Call,				//src1 should be func id / address (funcID / reg / imm)
		//CallExternal,		//src1 should be func name (in symbol table)	(DEPRECATED)
		Return,				//return with value src1
		Allocate,			//alloc a var in stack with size of src1 bytes and align to src2 bytes
	};

	struct Operand
	{
		enum OperandType : std::uint8_t
		{
			empty,
			reg64, reg32, reg16, reg8, 
			imm64, imm32, imm16, imm8,
			funcID, constID, globalVarID, externalSymbolName
		};
		OperandType type;
		std::uint64_t val;
		std::size_t ver;

		bool isImm() const { return type == imm64 || type == imm32 || type == imm16 || type == imm8; }
		bool isReg() const { return type == reg64 || type == reg32 || type == reg16 || type == reg8; }
		bool isConst() const { return type != empty && !isReg(); }
		size_t size() const
		{
			switch (type)
			{
			case reg64: case imm64:
				return 8;
			case reg32: case imm32:
				return 4;
			case reg16: case imm16:
				return 2;
			case reg8: case imm8:
				return 1;
			default:
				return POINTER_SIZE;
			}
		}
	};
	//NOTE: when writing one part of the 64-bit register, the value in the other part of the register is UNDEFINED
	inline Operand Reg64(std::uint64_t regid) { return Operand{ Operand::reg64, regid }; }
	inline Operand Reg32(std::uint64_t regid) { return Operand{ Operand::reg32, regid }; }
	inline Operand Reg16(std::uint64_t regid) { return Operand{ Operand::reg16, regid }; }
	inline Operand Reg8(std::uint64_t regid) { return Operand{ Operand::reg8, regid }; }
	inline Operand RegPtr(std::uint64_t regid) { return Reg64(regid); }

	inline Operand Imm64(std::int64_t num) { return Operand{ Operand::imm64, std::uint64_t(num) }; }
	inline Operand Imm32(std::int32_t num) { return Operand{ Operand::imm32, std::uint64_t(num) }; }
	inline Operand Imm16(std::int16_t num) { return Operand{ Operand::imm16, std::uint64_t(num) }; }
	inline Operand Imm8(std::int8_t num) { return Operand{ Operand::imm8, std::uint64_t(num) }; }
	inline Operand ImmPtr(std::int64_t num) { return Imm64(num); }

	inline Operand IDFunc(size_t funcID) { return Operand{ Operand::funcID, funcID }; }
	inline Operand IDConst(size_t constID) { return Operand{ Operand::constID, constID }; }
	inline Operand IDGlobalVar(size_t varID) { return Operand{ Operand::globalVarID, varID }; }
	inline Operand IDExtSymbol(size_t symbolID) { return Operand{ Operand::externalSymbolName, symbolID }; }
	inline Operand EmptyOperand() { return Operand{ Operand::empty }; }

	struct Instruction
	{
		Operation oper;
		Operand dst, src1, src2;
		std::vector<Operand> paramExt;
	};

	enum BranchFreq : std::uint64_t
	{
		normal = 0, likely = 1, unlikely = 2
	};

	inline Instruction IRNop() { return Instruction{ Nop }; }
	inline Instruction IR(Operand dst, Operation oper, Operand src) { return Instruction{ oper, dst, src }; }
	inline Instruction IR(Operand dst, Operation oper, Operand src1, Operand src2) { return Instruction{ oper, dst, src1, src2 }; }
	//[[deprecated]] inline Instruction IR(Operand dst, Operation oper, Operand src, std::vector<Operand> paramExt) { return Instruction{ oper, dst, src, Operand(), paramExt }; }
	inline Instruction IRCall(Operand dst, Operand func, std::vector<Operand> paramExt) { return Instruction{ Call, dst, func, Operand(), paramExt }; }
	inline Instruction IRJump() { return Instruction{ Jump }; }
	inline Instruction IRReturn() { return Instruction{ Return , EmptyOperand(), EmptyOperand()}; }
	inline Instruction IRReturn(Operand ret) { return Instruction{ Return, EmptyOperand(), ret }; }
	inline Instruction IRBranch(Operand src, BranchFreq freq = normal) { return Instruction{ Br, Operand{}, src, Imm64(freq)}; }
	inline Instruction IRStore(Operand data, Operand addr) { return Instruction{ Store, data, addr }; }
	inline Instruction IRStoreA(Operand data, Operand base, Operand offset) { return Instruction{ StoreA, data, base, offset }; }

	struct PhiIns
	{
		Operand dst;
		std::vector<Operand> srcs;
	};

	//FIXME: Possible Memory Leak
	class Block
	{
	public:
		std::list<Instruction> ins;
		std::shared_ptr<Block> brTrue, brFalse;
		std::vector<PhiIns> phi;

	public:
		void traverse(std::function<bool(Block *)> func)
		{
			std::set<Block *> visited;
			std::queue<Block *> q;
			q.push(this);
			visited.insert(this);
			while (!q.empty())
			{
				Block *cur = q.front();
				q.pop();
				if (!func(cur))
					return;
				if (cur->brTrue && visited.find(cur->brTrue.get()) == visited.end())
				{
					visited.insert(cur->brTrue.get());
					q.push(cur->brTrue.get());
				}
				if (cur->brFalse && visited.find(cur->brFalse.get()) == visited.end())
				{
					visited.insert(cur->brFalse.get());
					q.push(cur->brFalse.get());
				}
			}
		}
	};

	class Function
	{
	public:
		std::vector<Operand> params;
		std::shared_ptr<Block> inBlock, outBlock;
	};
}

#endif