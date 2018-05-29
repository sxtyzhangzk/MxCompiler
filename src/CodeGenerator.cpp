#include "common_headers.h"
#include "CodeGenerator.h"
#include "RegisterAllocatorSSA.h"
#include "ASM.h"
#include "InstructionSelect.h"
using namespace MxIR;

const std::vector<int> CodeGenerator::regCallerSave = { 0, 10, 11, 7, 6, 2, 1, 8, 9 };	//rax r10 r11 rdi rsi rdx rcx r8 r9
const std::vector<int> CodeGenerator::regCalleeSave = { 3, 12, 13, 14, 15 };			//rbx r12-r15 (rbp)
const std::vector<int> CodeGenerator::regParam = { 7, 6, 2, 1, 8, 9 };

void CodeGenerator::generateFunc(MxProgram::funcInfo &finfo, const std::string &label)
{
	func = &finfo.content;
	bool hasFuncCall = false;
	varID = 0;
	func->inBlock->traverse([&hasFuncCall, this](Block *block) -> bool
	{
		for (auto &ins : block->ins)
		{
			for (Operand *operand : join<Operand *>(ins.getInputReg(), ins.getOutputReg()))
				varID = std::max(varID, operand->val);

			if (ins.oper == Call)
				hasFuncCall = true;
		}
		return true;
	});

	std::vector<int> regList;
	if (hasFuncCall)
	{
		std::copy(regCalleeSave.begin(), regCalleeSave.end(), std::back_inserter(regList));
		std::copy(regCallerSave.begin(), regCallerSave.end(), std::back_inserter(regList));
	}
	else
	{
		std::copy(regCallerSave.begin(), regCallerSave.end(), std::back_inserter(regList));
		std::copy(regCalleeSave.begin(), regCalleeSave.end(), std::back_inserter(regList));
	}
	
	InstructionSelect is(*func);
	is.work();

	regularizeInsnPre();
	initFuncEntryExit();
	setRegisterConstrains();
	setRegisterPrefer();

	RegisterAllocatorSSA regAllocator(finfo.content, regList);
	regAllocator.work();

	regularizeInsnPost();
	func->mergeBlocks();

	allocateStackFrame();

	writeLabel(label);
	if (hasFuncCall || stackSize > 0)
	{
		writeCode("push rbp");
		writeCode("mov rbp, rsp");
		popRBP = true;
	}
	else
	{
		for (auto &kv : stackFrame)
			kv.second -= 8;
		popRBP = false;
	}
	if (stackSize > 0)
		writeCode("sub rsp, ", stackSize);

	translateBlocks(sortBlocks(func->inBlock.get()));
}

void CodeGenerator::initFuncEntryExit()
{
	std::vector<Operand> pMoveDst, pMoveSrc;
	for (int reg : regCalleeSave)
	{
		pMoveDst.push_back(RegPtr(++varID).setPRegID(reg));
		pMoveSrc.push_back(RegPtr(Operand::InvalidID).setPRegID(reg));
	}
	func->inBlock->ins.push_front(IRParallelMove(pMoveDst, pMoveSrc));
	for (Operand &operand : pMoveDst)
		operand.pregid = -1;
	for (Block *exit : func->outBlock->preds)
	{
		assert(exit->ins.back().oper == Return);
		if (exit->ins.back().src1.isReg())
			exit->ins.insert(std::prev(exit->ins.end()), IR(EmptyOperand(), MoveToRegister, exit->ins.back().src1.clone().setPRegID(0)));
		else if (exit->ins.back().src1.type != Operand::empty)
			exit->ins.insert(std::prev(exit->ins.end()), IR(RegSize(Operand::InvalidID, exit->ins.back().src1.size()).setPRegID(0), Move, exit->ins.back().src1));
		exit->ins.insert(std::prev(exit->ins.end()), IRParallelMove(pMoveSrc, pMoveDst));
	}

	pMoveDst.clear();
	pMoveSrc.clear();
	for (size_t i = 0; i < regParam.size() && i < func->params.size(); i++)
	{
		pMoveDst.push_back(func->params[i].clone().setPRegID(regParam[i]));
		pMoveSrc.push_back(RegPtr(Operand::InvalidID).setPRegID(regParam[i]));
	}
	func->inBlock->ins.push_front(IRParallelMove(pMoveDst, pMoveSrc));
	for (size_t i = regParam.size(); i < func->params.size(); i++)
		func->inBlock->ins.push_front(IR(func->params[i], ExternalVar, ImmPtr((std::int64_t(i) - 4) * 8)));
}

void CodeGenerator::regularizeInsnPre()
{
	static const std::set<Operation> commutativeOp = { Add, Mult, And, Or, Xor, Seq, Sne, LoadA, StoreA };
	static const std::map<Operation, Operation> semiCommutiveOp = {
		{Slt, Sgt}, {Sle, Sge}, {Sgt, Slt}, {Sge, Sle},
		{Sltu, Sgtu}, {Sleu, Sgeu}, {Sgtu, Sltu}, {Sgeu, Sleu},
	};
	func->inBlock->traverse([this](Block *block) -> bool
	{
		for (auto iter = block->ins.begin(); iter != block->ins.end(); ++iter)
		{
			if (!iter->src1.isConst() || iter->oper == Move || iter->oper == Load || iter->oper == Store || iter->oper == Call || iter->oper == Allocate)
				continue;
			if (iter->oper == Sext)
			{
				iter->src1 = ImmSize(iter->src1.val, iter->dst.size());
				iter->oper = Move;
			}
			else if (iter->oper == Zext)
			{
				iter->src1.val &= (1ULL << iter->dst.size() * 8) - 1;
				iter->oper = Move;
			}
			else if (commutativeOp.count(iter->oper) && !iter->src2.isConst())
				std::swap(iter->src1, iter->src2);
			else if (semiCommutiveOp.count(iter->oper) && !iter->src2.isConst())
				std::swap(iter->src1, iter->src2), iter->oper = semiCommutiveOp.find(iter->oper)->second;
			else
			{
				Operand tmpReg = RegSize(++varID, iter->src1.size());
				block->ins.insert(iter, IR(tmpReg, Move, iter->src1));
				iter->src1 = tmpReg;
			}
		}
		for (auto iter = block->ins.begin(); iter != block->ins.end(); ++iter)
		{
			if (!iter->src2.isConst() || iter->oper == Allocate)
				continue;
			if (!iter->src2.isImm() 
				|| (iter->src2.type == Operand::imm64 && (std::int64_t(iter->src2.val) > INT32_MAX || std::int64_t(iter->src2.val) < INT32_MIN))
				|| iter->oper == Div
				|| iter->oper == Mod)
			{
				Operand tmpReg = RegSize(++varID, iter->src2.size());
				block->ins.insert(iter, IR(tmpReg, Move, iter->src2));
				iter->src2 = tmpReg;
			}
		}
		return true;
	});
}

void CodeGenerator::setRegisterConstrains()
{
	func->inBlock->traverse([this](Block *block) -> bool
	{
		for (auto iter = block->ins.begin(); iter != block->ins.end(); ++iter)
		{
			if (iter->oper == Div || iter->oper == Mod)
			{
				Operand tmp = RegSize(++varID, iter->src1.size());
				block->ins.insert(iter, IR(tmp, Move, iter->src1));
				block->ins.insert(iter, IRMoveToRegister({ 
					tmp.clone().setPRegID(0), 
					RegPtr(Operand::InvalidID).setPRegID(2),
				}));
				block->ins.insert(iter, IRLockRegister({ 0, 2 }));
				iter->src1 = EmptyOperand();
				iter->dst.pregid = iter->oper == Div ? 0 : 2;
				block->ins.insert(std::next(iter), IRUnlockRegister());
			}
			else if (iter->oper == Shlu || iter->oper == Shru || iter->oper == Shl || iter->oper == Shr)
			{
				if (iter->src2.isReg())
				{
					block->ins.insert(iter, IRMoveToRegister({
						iter->src2.clone().setPRegID(1),
					}));
					block->ins.insert(iter, IRLockRegister({ 1 }));
					iter->src2 = EmptyOperand();
					block->ins.insert(std::next(iter), IRUnlockRegister());
				}
			}
			else if (iter->oper == Call)
			{
				size_t deltaRSP = 0;
				if (iter->paramExt.size() > regParam.size() && (iter->paramExt.size() - regParam.size()) % 2 == 1)
					block->ins.insert(iter, IR(EmptyOperand(), Placeholder, ImmPtr(0), ImmPtr(8))), deltaRSP += 8;	//placeholder(0, 8): sub rsp, 8
				for (size_t i = regParam.size(); i < iter->paramExt.size(); i++)
					block->ins.insert(iter, IR(EmptyOperand(), PushParam, iter->paramExt[i])), deltaRSP += 8;
				std::vector<Operand> movRegParam;
				for (size_t i = 0; i < regParam.size() && i < iter->paramExt.size(); i++)
				{
					Operand tmp = RegSize(++varID, iter->paramExt[i].size());
					block->ins.insert(iter, IR(tmp, Move, iter->paramExt[i]));
					movRegParam.push_back(tmp.clone().setPRegID(regParam[i]));
				}
				for (int reg : regCallerSave)
				{
					if (std::find_if(movRegParam.begin(), movRegParam.end(),
						[reg](Operand operand) { return operand.pregid == reg; }) == movRegParam.end())
					{
						movRegParam.push_back(RegPtr(Operand::InvalidID).setPRegID(reg));
					}
				}
				block->ins.insert(iter, IRMoveToRegister(movRegParam));
				block->ins.insert(iter, IRLockRegister(regCallerSave));
				iter->paramExt.clear();
				iter->dst.pregid = 0;
				if (deltaRSP > 0)
					block->ins.insert(std::next(iter), IR(EmptyOperand(), Placeholder, ImmPtr(1), ImmPtr(deltaRSP)));	//placeholder(1, delta): add rsp, delta
				block->ins.insert(std::next(iter), IRUnlockRegister());
			}
		}
		return true;
	});
}

void CodeGenerator::setRegisterPrefer()
{
	static const std::set<Operation> preferAny = {Add, Mult, And, Or, Xor, Neg, Not, Move };
	static const std::set<Operation> preferFirst = { Sub, Shlu, Shru, Shl, Shr, ParallelMove };
	func->inBlock->traverse([](Block *block) -> bool
	{
		for (auto &ins : block->ins)
		{
			if (preferAny.count(ins.oper))
				ins.hint = InstructionBase::PreferAnyOfOperands;
			else if (preferFirst.count(ins.oper))
				ins.hint = InstructionBase::PreferCorrespondingOperand;
			else
				ins.hint = InstructionBase::NoPrefer;
		}
		return true;
	});
}

void CodeGenerator::regularizeInsnPost()
{
	static const std::set<Operation> targetOp = { Add, Sub, Mult, Shlu, Shru, Shl, Shr, And, Or, Xor, Neg, Not };
	static const std::set<Operation> commutativeOp = { Add, Mult, And, Or, Xor };
	func->inBlock->traverse([](Block *block) -> bool
	{
		for (auto iter = block->ins.begin(); iter != block->ins.end(); )
		{
			if (iter->oper == Move)
			{
				if (iter->src1.isReg() && iter->dst.pregid == iter->src1.pregid)
					iter = block->ins.erase(iter);
				else
					++iter;
			}
			else if (iter->oper == ParallelMove)
			{
				struct opInfo
				{
					size_t nNext;
					int from;
					Operand operand;
					opInfo() : nNext(0), from(-1) {}
				};
				std::map<int, opInfo> info;
				for (size_t i = 0, j = iter->paramExt.size() / 2; j < iter->paramExt.size(); i++, j++)
				{
					// data: paramExt[i] <- paramExt[j]
					if (iter->paramExt[i].pregid != -1 && iter->paramExt[j].pregid != -1 &&
						iter->paramExt[i].pregid != iter->paramExt[j].pregid)
					{
						assert(iter->paramExt[i].size() == iter->paramExt[j].size());
						int regDst = iter->paramExt[i].pregid, regSrc = iter->paramExt[j].pregid;
						assert(info[regDst].from == -1);
						info[regDst].from = regSrc;
						info[regDst].operand = iter->paramExt[i];
						info[regSrc].nNext++;
					}
				}
				std::queue<int> Q;
				for (auto &kv : info)
					if (kv.second.nNext == 0)
						Q.push(kv.first);
				while (!Q.empty())
				{
					int reg = Q.front();
					Q.pop();
					assert(info.count(reg));
					if (info[reg].from != -1)
					{
						assert(info.count(info[reg].from));
						block->ins.insert(iter, IR(
							RegPtr(Operand::InvalidID).setPRegID(info[reg].operand.pregid),
							Move,
							RegPtr(Operand::InvalidID).setPRegID(info[reg].from)
						));
						if (--info[info[reg].from].nNext == 0)
							Q.push(info[reg].from);
					}
					info.erase(reg);
				}
				while (!info.empty())
				{
					int head = info.begin()->first;
					int now = info[head].from;
					do
					{
						assert(info.count(now));
						block->ins.insert(iter, IR(EmptyOperand(), Xchg, 
							RegPtr(Operand::InvalidID).setPRegID(info[now].operand.pregid), 
							RegPtr(Operand::InvalidID).setPRegID(info[now].from)));
						int tmp = now;
						now = info[now].from;
						info.erase(tmp);
					} while (now != head);
					info.erase(head);
				}
				iter = block->ins.erase(iter);
			}
			else if (targetOp.count(iter->oper))
			{
				assert(iter->src1.isReg());
				if (iter->dst.pregid == iter->src1.pregid)
					++iter;
				else if (commutativeOp.count(iter->oper) && iter->src2.isReg() && iter->dst.pregid == iter->src2.pregid)
				{
					std::swap(iter->src1, iter->src2);
					++iter;
				}
				else
				{
					if (!iter->src2.isReg() || iter->dst.pregid != iter->src2.pregid)
					{
						block->ins.insert(iter, IR(iter->dst, Move, iter->src1));
						iter->src1 = iter->dst;
					}
					else
					{
						assert(iter->oper == Sub);
						std::swap(iter->src1, iter->src2);
						block->ins.insert(std::next(iter), IR(iter->dst, Neg, iter->dst));
					}
					++iter;
				}
			}
			else
				++iter;
		}
		return true;
	});
}

void CodeGenerator::allocateStackFrame()
{
	ssize_t curOffset = 0;
	for (auto &ins : func->inBlock->ins)
	{
		if (ins.oper == Allocate)
		{
			ssize_t addr;
			assert(ins.src1.isImm() && ins.src2.isImm());
			if (ins.paramExt.empty())
			{
				curOffset += ins.src1.val;
				curOffset = alignAddr(curOffset, ins.src2.val);
				addr = -curOffset;
			}
			else
				addr = ins.paramExt[0].val;

			if (ins.dst.noreg)
				stackFrame[ins.dst.val] = addr;
			else
				ins = IR(ins.dst, LoadAddr, RegPtr(Operand::InvalidID).setPRegID(5), ImmPtr(addr));
		}
	}
	curOffset = alignAddr(curOffset, 16);
	stackSize = curOffset;
}

std::string CodeGenerator::getOperand(Operand operand)
{
	if (operand.isConst())
		return getConst(operand, true, -1);
	assert(operand.isReg());
	if(operand.pregid != -1)
		return regName(operand.pregid, operand.size());
	if (stackFrame.count(operand.val))
	{
		std::stringstream ss;
		if (stackFrame[operand.val] > 0)
			ss << (popRBP ? "rbp+" : "rsp+") << stackFrame[operand.val];
		else
			ss << (popRBP ? "rbp-" : "rsp-") << -stackFrame[operand.val];
		return ss.str();
	}
	assert(false);
	return "";
}

void CodeGenerator::translateIns(Instruction ins)
{
	if (ins.oper == Placeholder)
	{
		assert(ins.src1.isImm());
		if (ins.src1.val == 0)
		{
			assert(ins.src2.isImm());
			writeCode("sub rsp, ", ins.src2.val);
		}
		else if (ins.src1.val == 1)
		{
			assert(ins.src2.isImm());
			writeCode("add rsp, ", ins.src2.val);
		}
		else
		{
			assert(false);
		}
		return;
	}

	static const std::unordered_map<int, std::string> mapInsBinary = {
		{ Add, "add" },{ Sub, "sub" },{ Mult, "imul" },
		{ And, "and" },{ Or, "or" },{ Xor, "xor" } };
	static const std::unordered_map<int, std::string> mapInsUnary = {
		{ Neg, "neg" },{ Not, "not" } };
	if (mapInsBinary.count(ins.oper))
	{
		assert(ins.src1.isReg() && ins.src1.pregid == ins.dst.pregid);
		writeCode(mapInsBinary.find(ins.oper)->second, " ", getOperand(ins.dst), ", ", getOperand(ins.src2));
		return;
	}
	if (mapInsUnary.count(ins.oper))
	{
		assert(ins.src1.isReg() && ins.src1.pregid == ins.dst.pregid);
		writeCode(mapInsUnary.find(ins.oper)->second, " ", getOperand(ins.dst));
		return;
	}

	if (ins.oper == Div || ins.oper == Mod)
	{
		assert(ins.src2.isReg());
		if (ins.src2.size() == 8)
			writeCode("cqo");
		else if (ins.src2.size() == 4)
			writeCode("cdq");
		else if (ins.src2.size() == 2)
			writeCode("cwd");
		writeCode("idiv ", getOperand(ins.src2));
		return;
	}

	if (ins.oper == Move)
	{
		writeCode("mov ", getOperand(ins.dst), ", ", getOperand(ins.src1));
		return;
	}
	if (ins.oper == Sext)
	{
		writeCode("movsx ", getOperand(ins.dst), ", ", getOperand(ins.src1));
		return;
	}
	if (ins.oper == Zext)
	{
		if (ins.src1.size() >= 4)
		{
			Operand tmp = ins.dst;
			tmp.type = ins.src1.type;
			writeCode("mov ", getOperand(tmp), ", ", getOperand(ins.src1));
		}
		else
			writeCode("movzx ", getOperand(ins.dst), ", ", getOperand(ins.src1));
		return;
	}
	if (ins.oper == Xchg)
	{
		writeCode("xchg ", getOperand(ins.src1), ", ", getOperand(ins.src2));
		return;
	}

	if (ins.oper == Load)
	{
		writeCode("mov ", getOperand(ins.dst), ", ", sizeName(ins.dst.size()), " [", getOperand(ins.src1), "]");
		return;
	}
	if (ins.oper == Store)
	{
		writeCode("mov ", sizeName(ins.dst.size()), " [", getOperand(ins.src1), "], ", getOperand(ins.dst));
		return;
	}
	if (ins.oper == LoadA)
	{
		if(ins.src2.isImm() && std::int64_t(ins.src2.val) < 0)
			writeCode("mov ", getOperand(ins.dst), ", ", sizeName(ins.dst.size()), " [", getOperand(ins.src1), "-", getOperand(ins.src2.clone().setVal(-std::int64_t(ins.src2.val))), "]");
		else
			writeCode("mov ", getOperand(ins.dst), ", ", sizeName(ins.dst.size()), " [", getOperand(ins.src1), "+", getOperand(ins.src2), "]");
		return;
	}
	if (ins.oper == StoreA)
	{
		if (ins.src2.isImm() && std::int64_t(ins.src2.val) < 0)
			writeCode("mov ", sizeName(ins.dst.size()), " [", getOperand(ins.src1), "-", getOperand(ins.src2.clone().setVal(-std::int64_t(ins.src2.val))), "], ", getOperand(ins.dst));
		else
			writeCode("mov ", sizeName(ins.dst.size()), " [", getOperand(ins.src1), "+", getOperand(ins.src2), "], ", getOperand(ins.dst));
		return;
	}

	if (ins.oper == LoadAddr)
	{
		if (ins.src2.isImm() && std::int64_t(ins.src2.val) < 0)
			writeCode("lea ", getOperand(ins.dst), ", [", getOperand(ins.src1), "-", getOperand(ins.src2.clone().setVal(-std::int64_t(ins.src2.val))), "]");
		else
			writeCode("lea ", getOperand(ins.dst), ", [", getOperand(ins.src1), "+", getOperand(ins.src2), "]");
		return;
	}

	if (ins.oper == Call)
	{
		writeCode("call ", getOperand(ins.src1));
		return;
	}
	if (ins.oper == PushParam)
	{
		if (ins.src1.isImm())
			writeCode("push qword ", getOperand(ins.src1));
		else if (ins.src1.isReg())
			writeCode("push ", regName(ins.src1.pregid, 8));
		else
			writeCode("push ", getOperand(ins.src1));
		return;
	}
	if (ins.oper == Return)
	{
		if (popRBP)
		{
			writeCode("mov rsp, rbp");
			writeCode("pop rbp");
		}
		if (ins.src1.type == Operand::empty)
			writeCode("xor rax, rax");
		writeCode("ret");
		return;
	}
	if (ins.oper == TestZero)
	{
		assert(ins.src1.isReg());
		writeCode("mov ", getOperand(ins.dst), ", ", getOperand(ins.src2));
		writeCode("test ", getOperand(ins.src1), ", ", getOperand(ins.src1));
		writeCode("cmovnz ", getOperand(ins.dst), ", ", getOperand(ins.src1));
		return;
	}

	static const std::unordered_map<int, std::string> mapInsCmp = {
		{ Slt, "setl" },{ Sle, "setle" },{ Seq, "sete" },{ Sne, "setne" },{ Sgt, "setg" },{ Sge, "setge" },
		{ Sltu, "setb" },{ Sleu, "setbe" },{ Sgtu, "seta" },{ Sgeu, "setae" } };
	if (mapInsCmp.count(ins.oper))
	{
		writeCode("cmp ", getOperand(ins.src1), ", ", getOperand(ins.src2));
		if(ins.dst.isReg())
			writeCode(mapInsCmp.find(ins.oper)->second, " ", getOperand(ins.dst));
		return;
	}

	if (ins.oper == Br)
	{
		if(ins.src1.isReg())
			writeCode("test ", getOperand(ins.src1), ", ", getOperand(ins.src1));
		return;
	}

	static const std::unordered_map<int, std::string> mapInsShift = {
		{ Shl, "sal" },{ Shr, "sar" },
		{ Shlu, "shl" },{ Shru, "shr" }, };
	if (mapInsShift.count(ins.oper))
	{
		if (ins.src2.isConst())
			writeCode(mapInsShift.find(ins.oper)->second, " ", getOperand(ins.src1), ", ", getOperand(ins.src2));
		else
			writeCode(mapInsShift.find(ins.oper)->second, " ", getOperand(ins.src1), ", cl");
		return;
	}
}