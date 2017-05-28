#ifndef MX_COMPILER_IR_H
#define MX_COMPILER_IR_H

#include "common.h"
#include "utils/JoinIterator.h"
#include "utils/ElementAdapter.h"

namespace MxIR
{
	enum Operation
	{
		Nop,
		Add, Sub,			//dst = src1 op src2, dst & src1 & src2 must have the same size
		Mult, Div, Mod,
		Shlu, Shru,
		Shl, Shr,			//FIXME: signed shift
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
		Allocate,			//alloc a var in stack with size of src1 bytes and align to src2 bytes, alloc hint store in paramExt[0]

		TestZero,			//dst = test_zero src1, src2: if src1 == 0, dst = src2; else dst = src1

		//Operations below are only avaliable when allocating register & generating code 
		LockReg,
		UnlockReg,
		ParallelMove,		//paramExt[0..n/2-1] : dst, paramExt[n/2, n] : src
		MoveToRegister,
		ExternalVar,		//dst = external(hint), define a var without loading it to register; alloc hint store in src1; must be in inBlock
		PushParam,
		Placeholder,
		Xchg,				//xchg(src1, src2)
		LoadAddr,			//lea(src1 + src2)
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
		std::size_t ver;		//in ssa form, ver >= 1
		int pregid;				//physical register id
		bool noreg;

		static const std::uint64_t InvalidID = std::uint64_t(-1);

		Operand() : type(empty), pregid(-1), ver(0), noreg(false) {}
		Operand(OperandType type, std::uint64_t val) : type(type), val(val), pregid(-1), ver(0), noreg(false) {}

		Operand clone() const { return Operand(*this); }
		Operand & setVer(size_t newVer) { ver = newVer; return *this; }
		Operand & setVal(std::uint64_t newVal) { val = newVal; return *this; }
		Operand & setPRegID(int newRegID) { pregid = newRegID; return *this; }
		Operand & setNOReg(bool newFlag) { noreg = newFlag; return *this; }

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

		//used in set/map of Operand
		bool operator<(const Operand &rhs) const
		{
			assert(isReg() && rhs.isReg());
			if (val == rhs.val)
				return ver < rhs.ver;
			return val < rhs.val;
		}
	};
	//NOTE: when writing one part of the 64-bit register, the value in the other part of the register is UNDEFINED
	inline Operand Reg64(std::uint64_t regid) { return Operand{ Operand::reg64, regid }; }
	inline Operand Reg32(std::uint64_t regid) { return Operand{ Operand::reg32, regid }; }
	inline Operand Reg16(std::uint64_t regid) { return Operand{ Operand::reg16, regid }; }
	inline Operand Reg8(std::uint64_t regid) { return Operand{ Operand::reg8, regid }; }
	inline Operand RegPtr(std::uint64_t regid) { return Reg64(regid); }
	inline Operand RegSize(std::uint64_t regid, size_t size)
	{
		switch (size)
		{
		case 1:
			return Reg8(regid);
		case 2:
			return Reg16(regid);
		case 4:
			return Reg32(regid);
		case 8:
			return Reg64(regid);
		default:
			assert(false);
			return Operand();
		}
	}

	inline Operand Imm64(std::int64_t num) { return Operand{ Operand::imm64, std::uint64_t(num) }; }
	inline Operand Imm32(std::int32_t num) { return Operand{ Operand::imm32, std::uint64_t(num) }; }
	inline Operand Imm16(std::int16_t num) { return Operand{ Operand::imm16, std::uint64_t(num) }; }
	inline Operand Imm8(std::int8_t num) { return Operand{ Operand::imm8, std::uint64_t(num) }; }
	inline Operand ImmPtr(std::int64_t num) { return Imm64(num); }
	inline Operand ImmSize(std::uint64_t val, size_t size)
	{
		Operand op;
		op.val = val;
		if (size == 1)
			op.type = Operand::imm8;
		else if (size == 2)
			op.type = Operand::imm16;
		else if (size == 4)
			op.type = Operand::imm32;
		else if (size == 8)
			op.type = Operand::imm64;
		else
		{
			assert(false);
		}
		return op;
	}

	inline Operand IDFunc(size_t funcID) { return Operand{ Operand::funcID, funcID }; }
	inline Operand IDConst(size_t constID) { return Operand{ Operand::constID, constID }; }
	inline Operand IDGlobalVar(size_t varID) { return Operand{ Operand::globalVarID, varID }; }
	inline Operand IDExtSymbol(size_t symbolID) { return Operand{ Operand::externalSymbolName, symbolID }; }
	inline Operand EmptyOperand() { return Operand(); }

	struct InstructionBase
	{
		enum registerHint { NoPrefer, PreferAnyOfOperands, PreferOperands, PreferCorrespondingOperand };
		registerHint hint;

		InstructionBase() : hint(NoPrefer) {}
		virtual std::vector<Operand *> getInputReg() = 0;
		virtual std::vector<Operand *> getOutputReg() = 0;
	};

	struct Instruction : public InstructionBase
	{
		Operation oper;
		Operand dst, src1, src2;
		std::vector<Operand> paramExt;

		Instruction() { autoPrefer(); }
		Instruction(Operation oper) : oper(oper) { autoPrefer(); }
		Instruction(Operation oper, Operand dst, Operand src1) : oper(oper), dst(dst), src1(src1) { autoPrefer(); }
		Instruction(Operation oper, Operand dst, Operand src1, Operand src2) : oper(oper), dst(dst), src1(src1), src2(src2) { autoPrefer(); }
		Instruction(Operation oper, Operand dst, Operand src1, Operand src2, const std::vector<Operand> &paramExt) : oper(oper), dst(dst), src1(src1), src2(src2), paramExt(paramExt) { autoPrefer(); }

		void autoPrefer()
		{
			if (oper == Move)
				hint = PreferAnyOfOperands;
			else if (oper == ParallelMove)
				hint = PreferCorrespondingOperand;
		}

		virtual std::vector<Operand *> getInputReg() override final
		{
			std::vector<Operand *> ret;
			if (oper == ParallelMove)
			{
				assert(paramExt.size() % 2 == 0);
				for (size_t i = paramExt.size() / 2; i < paramExt.size(); i++)
				{
					assert(paramExt[i].isReg());
					ret.push_back(&paramExt[i]);
				}
				return ret;
			}
			if ((oper == Store || oper == StoreA) && dst.isReg())
				ret.push_back(&dst);
			if (src1.isReg())
				ret.push_back(&src1);
			if (src2.isReg())
				ret.push_back(&src2);
			for (auto &param : paramExt)
				if (param.isReg())
					ret.push_back(&param);
			return ret;
		}
		virtual std::vector<Operand *> getOutputReg() override final
		{
			std::vector<Operand *> ret;
			if (oper == ParallelMove)
			{
				assert(paramExt.size() % 2 == 0);
				for (size_t i = 0; i < paramExt.size() / 2; i++)
				{
					assert(paramExt[i].isReg());
					ret.push_back(&paramExt[i]);
				}
				return ret;
			}
			if (oper != Store && oper != StoreA && dst.isReg())
				ret.push_back(&dst);
			return ret;
		}
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

	inline Instruction IRParallelMove(const std::vector<Operand> &dst, const std::vector<Operand> &src)
	{
		assert(dst.size() == src.size());
		Instruction insn(ParallelMove);
		for (Operand operand : dst)
			insn.paramExt.push_back(operand);
		for (Operand operand : src)
			insn.paramExt.push_back(operand);
		return insn;
	}
	inline Instruction IRMoveToRegister(const std::vector<Operand> &param) { return Instruction{ MoveToRegister, EmptyOperand(), EmptyOperand(), EmptyOperand(), param }; }
	inline Instruction IRLockRegister(const std::vector<int> &regs)
	{
		Instruction insn(LockReg);
		for (int reg : regs)
			insn.paramExt.push_back(RegPtr(Operand::InvalidID).setPRegID(reg));
		return insn;
	}
	inline Instruction IRUnlockRegister() { return Instruction(UnlockReg); }

	class Block;
	struct PSTNode
	{
		Block *inBlock, *outBlock;
		std::set<Block *> blocks;

		std::list<std::shared_ptr<PSTNode>> children;
		std::list<std::shared_ptr<PSTNode>>::iterator iterParent;
		std::weak_ptr<PSTNode> parent, self;

		void traverse(std::function<void(PSTNode *)> func);
		std::set<Block *> getBlocks();

		bool isSibling(PSTNode *other) const { return parent.lock() == other->parent.lock(); }
		bool isSingleBlock() const
		{
			return inBlock == outBlock && blocks.size() == 1 && children.empty();
		}
		
	};
	//FIXME: Possible Memory Leak
	class Block
	{
	public:
		struct PhiIns : public InstructionBase
		{
			Operand dst;
			std::vector<std::pair<Operand, std::weak_ptr<Block>>> srcs;

			PhiIns() { hint = PreferOperands; }
			PhiIns(Operand dst) : dst(dst) { hint = PreferOperands; }

			virtual std::vector<Operand *> getInputReg() override final
			{
				std::vector<Operand *> ret;
				for (auto &src : srcs)
				{
					if(src.first.isReg())
						ret.push_back(&src.first);
				}
				return ret;
			}
			virtual std::vector<Operand *> getOutputReg() override final
			{
				assert(dst.isReg());
				return { &dst };
			}
		};
		struct SigmaIns : public InstructionBase
		{
			Operand dstTrue, dstFalse;
			Operand src;

			SigmaIns() {}
			SigmaIns(Operand src) : src(src) {}

			virtual std::vector<Operand *> getInputReg() override final
			{
				assert(src.isReg());
				return { &src };
			}
			virtual std::vector<Operand *> getOutputReg() override final
			{
				assert(dstTrue.isReg());
				assert(dstFalse.isReg());
				return { &dstTrue, &dstFalse };
			}
		};
		class block_ptr
		{
		public:
			explicit block_ptr(Block *curBlock) : curBlock(curBlock) 
			{
				assert(this == &curBlock->brTrue || this == &curBlock->brFalse);
			}
			block_ptr(const block_ptr &other) = delete;
			block_ptr(block_ptr &&other) = delete;

			~block_ptr();
			void reset();
			block_ptr & operator=(const block_ptr &other);
			block_ptr & operator=(const std::shared_ptr<Block> &block);
			block_ptr & operator=(std::shared_ptr<Block> &&block);
			bool operator==(const block_ptr &rhs) const { return ptr == rhs.ptr; }
			bool operator!=(const block_ptr &rhs) const { return ptr != rhs.ptr; }
			Block * operator->() const { return get(); }
			Block * get() const { return ptr.get(); }
			operator bool() const { return bool(ptr); }

		protected:
			std::shared_ptr<Block> ptr;
			std::list<Block *>::iterator iterPred;
			Block * const curBlock;
		};

	public:
		std::list<Instruction> ins;
		std::map<size_t, PhiIns> phi;		//map register id to phi instruction
		std::map<size_t, SigmaIns> sigma;

		block_ptr brTrue, brFalse;
		std::weak_ptr<Block> self;
		std::list<Block *> preds;

		std::weak_ptr<PSTNode> pstNode;

		IF_DEBUG(std::string dbgInfo);

	public:
		static std::shared_ptr<Block> construct();
		void traverse(std::function<bool(Block *)> func);
		void traverse_preorder(std::function<bool(Block *)> func);
		void traverse_postorder(std::function<bool(Block *)> func);
		void traverse_rev_postorder(std::function<bool(Block *)> func);
		auto instructions()
		{
			auto pair_second = [](auto &p) -> auto& { return p.second; };
			return join<InstructionBase>(element_adapter(phi, pair_second), ins, element_adapter(sigma, pair_second));
		}
		void redirectPhiSrc(Block *from, Block *to);

	protected:
		std::list<Block *>::iterator newPred(Block *pred);
		void removePred(std::list<Block *>::iterator iterPred);

	private:
		Block() : brTrue(this), brFalse(this) {}
	};

	class Function
	{
	public:
		std::vector<Operand> params;
		std::shared_ptr<Block> inBlock, outBlock;

		std::shared_ptr<PSTNode> pstRoot;

		void constructPST();
		void splitProgramRegion();
		void mergeBlocks();
		Function clone();
	};
}

#endif