#include "common_headers.h"
#include "CodeGeneratorBasic.h"
#include "ASM.h"
using namespace MxIR;

const std::string CodeGeneratorBasic::paramReg[] = { "rdi", "rsi", "rdx", "rcx", "r8", "r9" };
const int CodeGeneratorBasic::paramRegID[] = { 7, 6, 2, 1, 8, 9 };

void CodeGeneratorBasic::createLabel()
{
	std::set<std::string> setNames;
	for (size_t i = 0; i< program->vFuncs.size();i++)
	{
		const MxProgram::funcInfo &finfo = program->vFuncs[i];
		if ((finfo.attribute & Builtin) || (finfo.attribute & Export))
		{
			std::string funcName = symbol->vSymbol[finfo.funcName];
			assert(setNames.find(funcName) == setNames.end());
			setNames.insert(funcName);
			labelFunc.push_back(funcName);
		}
		else
		{
			std::string decName = decorateFuncName(finfo);
			if (setNames.find(decName) != setNames.end())
			{
				std::stringstream ss(decName);
				ss << "_" << i;
				decName = ss.str();
				assert(setNames.find(decName) == setNames.end());
			}
			setNames.insert(decName);
			labelFunc.push_back(decName);
		}
	}
	for (size_t i = 0; i < program->vGlobalVars.size(); i++)
	{
		std::string varName = "_GV_" + symbol->vSymbol[program->vGlobalVars[i].varName];
		assert(setNames.find(varName) == setNames.end());
		setNames.insert(varName);
		labelVar.push_back(varName);
	}
}

std::string CodeGeneratorBasic::decorateFuncName(const MxProgram::funcInfo &finfo)
{
	static const std::unordered_map<int, char> mapDec = {
		{MxType::Void, 'v'}, {MxType::Bool, 'b'}, {MxType::Integer, 'i'}, {MxType::String, 's'},
		{MxType::Object, 'o'}, {MxType::Function, 'f'} };
	std::stringstream ss;
	if (finfo.isThiscall)
	{
		if (finfo.paramType[0].className != size_t(-1))
			ss <<  "_M" << symbol->vSymbol[finfo.paramType[0].className] << "_";
		else
			ss << "_M__";
	}
	else
		ss << "_G_";
	ss << symbol->vSymbol[finfo.funcName] << "_";
	if (finfo.paramType.empty())
		ss << "v";
	for (auto &p : finfo.paramType)
	{
		ss << mapDec.find(p.mainType)->second;
		if (p.arrayDim > 0)
			ss << p.arrayDim;
	}
	return ss.str();
}

void CodeGeneratorBasic::generateFunc(MxProgram::funcInfo &finfo, const std::string &label)
{
	writeLabel(label);

	std::vector<size_t> regSize;
	auto checkReg = [&regSize](Operand operand)
	{
		if (!operand.isReg())
			return;
		if (regSize.size() <= operand.val)
			regSize.resize(operand.val + 1, 0);
		if (operand.size() > regSize[operand.val])
			regSize[operand.val] = operand.size();
	};
	for (auto &p : finfo.content.params)
		checkReg(p);
	finfo.content.inBlock->traverse([&](Block *block) -> bool
	{
		for (auto &ins : block->ins)
		{
			checkReg(ins.dst);
			checkReg(ins.src1);
			checkReg(ins.src2);
			for (auto &p : ins.paramExt)
				checkReg(p);
			if (ins.oper == Allocate)
			{
				assert(ins.src1.isImm() && ins.src2.isImm());
				allocAddr.push_back(std::make_tuple(ins.src1.val, ins.src2.val, 0));
			}
		}
		return true;
	});

	std::set<std::uint64_t> paramInStack;
	for (size_t i = 6; i < finfo.content.params.size(); i++)
		paramInStack.insert(finfo.content.params[i].val);

	regAddr.resize(regSize.size(), 0);
	std::uint64_t curOffset = 0;
	for (size_t i = 0; i < regSize.size(); i++)
	{
		if (paramInStack.count(i) > 0)
			continue;
		if (regSize[i] == 0)
			continue;
		std::uint64_t regOffset = curOffset + regSize[i];
		regOffset = alignAddr(regOffset, regSize[i]);
		regAddr[i] = regOffset;
		curOffset = regOffset;
	}
	for (auto &alloc : allocAddr)
	{
		std::uint64_t allocOffset = curOffset + std::get<0>(alloc);
		allocOffset = alignAddr(allocOffset, std::get<1>(alloc));
		std::get<2>(alloc) = allocOffset;
		curOffset = allocOffset;
	}
	curOffset = alignAddr(curOffset, 16);

	for (size_t i = 6; i < finfo.content.params.size(); i++)
		regAddr[finfo.content.params[i].val] = -(std::int64_t(i) - 4) * 8;

	// -- param[6] 64 bit -- <- rbp-16
	// -- ret addr 64 bit -- <- rbp-8
	// -- old rbp  64 bit -- <- rbp

	writeCode("push rbp");
	writeCode("mov rbp, rsp");
	//writeCode("push r12");
	//writeCode("push r13");
	writeCode("sub rsp, ", curOffset);

	for (size_t i = 0; i < 6 && i < finfo.content.params.size(); i++)
		writeCode("mov ", getVReg(finfo.content.params[i]), ", ", regName(paramRegID[i], finfo.content.params[i].size()));

	translateBlocks(sortBlocks(finfo.content.inBlock.get()));
}

void CodeGeneratorBasic::generateConst(const MxProgram::constInfo &cinfo, const std::string &label)
{
	assert(cinfo.labelOffset < cinfo.data.size());

	if (cinfo.align > 1)
		writeCode("align ", cinfo.align);

	auto encodeConst = [&](auto iterBegin, auto iterEnd)
	{
		std::string lastVisible;
		std::stringstream ss;
		for (auto iter = iterBegin; iter != iterEnd; ++iter)
		{
			if (*iter >= 0x20 && *iter <= 0x7E && *iter != '\'')
				lastVisible += *iter;
			else
			{
				if (!lastVisible.empty())
				{
					ss << "'" << lastVisible << "', ";
					lastVisible.clear();
				}
				ss << std::uint32_t(*iter) << ", ";
			}
		}
		if (!lastVisible.empty())
			ss << "'" << lastVisible << "', ";
		if (ss.str().empty())
			return;
		size_t length = ss.str().length();
		assert(length >= 2);
		writeCode("db ", ss.str().substr(0, length - 2));
	};

	encodeConst(cinfo.data.begin(), cinfo.data.begin() + cinfo.labelOffset);
	writeLabel(label);
	encodeConst(cinfo.data.begin() + cinfo.labelOffset, cinfo.data.end());
}

void CodeGeneratorBasic::generateVar(const MxProgram::varInfo &vinfo, const std::string &label)
{
	//TODO: align global var
	writeLabel(label);
	writeCode("resb ", vinfo.varType.getSize());
}

std::vector<Block *> CodeGeneratorBasic::sortBlocks(Block *inBlock)
{
	std::set<Block *> sorted;
	std::queue<Block *> q;
	std::vector<Block *> ret;
	inBlock->traverse([&](Block *blk)
	{
		if(!blk->ins.empty())
			q.push(blk); 
		return true; 
	});
	//assert(*unsorted.begin() == inBlock);

	auto trySort = [&](Block *blk)
	{
		if (!blk || blk->ins.empty())
			return false;
		auto iter = sorted.find(blk);
		if (iter != sorted.end())
			return false;
		ret.push_back(blk);
		sorted.insert(blk);
		return true;
	};

	while(true)
	{
		Block *now;
		do
		{
			if (q.empty())
				return ret;
			now = q.front();
			q.pop();
		} while (sorted.count(now) > 0);

		assert(!ret.empty() || now == inBlock);
		ret.push_back(now);
		sorted.insert(now);
		while (true)
		{
			assert(!now->ins.empty());
			if (now->ins.back().oper == Br && now->ins.back().src2.val == unlikely)
			{
				if (trySort(now->brFalse.get()))
					now = now->brFalse.get();
				else if (trySort(now->brTrue.get()))
					now = now->brTrue.get();
				else
					break;
			}
			else
			{
				if (trySort(now->brTrue.get()))
					now = now->brTrue.get();
				else if (trySort(now->brFalse.get()))
					now = now->brFalse.get();
				else
					break;
			}
		}
	}
}

void CodeGeneratorBasic::translateBlocks(const std::vector<MxIR::Block *> &vBlocks)
{
	static const std::unordered_map<int, std::pair<std::string, std::string>> mapInsCmp = {
		{ Slt, {"jl", "jge"} },{ Sle, {"jle", "jg"} },{ Seq, {"je", "jne"} }, { Sne, {"jne", "je"} },
		{ Sgt, {"jg", "jle"} }, { Sge, {"jge", "jl"} },
		{ Sltu, {"jb", "jae"} },{ Sleu, {"jbe", "ja"} },{ Sgtu, {"ja", "jbe"} },{ Sgeu, {"jae", "jb"} }};

	std::map<Block *, size_t> mapBlocks;
	for (size_t i = 0; i < vBlocks.size(); i++)
		mapBlocks.insert({ vBlocks[i], i });
	for (size_t idx = 0; idx < vBlocks.size(); idx++)
	{
		writeLabel(".L", cntLocalLabel + idx);
		Block *block = vBlocks[idx];
		for (auto i = block->ins.begin(); i != block->ins.end(); ++i)
		{
			if (i->oper == Jump)
			{
				assert(i == --block->ins.end());
				size_t nextIdx = mapBlocks.find(block->brTrue.get())->second;
				if (nextIdx != idx + 1)
					writeCode("jmp .L", cntLocalLabel + nextIdx);
				break;
			}
			if (i->oper == Br)
			{
				assert(i == --block->ins.end());
				size_t trueIdx = mapBlocks.find(block->brTrue.get())->second;
				size_t falseIdx = mapBlocks.find(block->brFalse.get())->second;

				if (i->src1.type == Operand::empty)
				{
					assert(block->ins.size() >= 2 && mapInsCmp.count(std::prev(i)->oper));
					std::string pos = mapInsCmp.find(std::prev(i)->oper)->second.first;
					std::string neg = mapInsCmp.find(std::prev(i)->oper)->second.second;
					if (trueIdx == idx + 1)
						writeCode(neg, " .L", cntLocalLabel + falseIdx);
					else if (falseIdx == idx + 1)
						writeCode(pos, " .L", cntLocalLabel + trueIdx);
					else
					{
						writeCode(neg, " .L", cntLocalLabel + falseIdx);
						writeCode("jmp .L", cntLocalLabel + trueIdx);
					}
				}
				else
				{
					translateIns(*i);
					if (trueIdx == idx + 1)
						writeCode("jz .L", cntLocalLabel + falseIdx);
					else if (falseIdx == idx + 1)
						writeCode("jnz .L", cntLocalLabel + trueIdx);
					else
					{
						writeCode("jz .L", cntLocalLabel + falseIdx);
						writeCode("jmp .L", cntLocalLabel + trueIdx);
					}
				}
				break;
			}
			translateIns(*i);
		}
	}
	cntLocalLabel += vBlocks.size();
}

std::string CodeGeneratorBasic::loadOperand(int id, MxIR::Operand src)
{
	std::string name = regName(id, src.size());
	loadOperand(name, src);
	return name;
}

void CodeGeneratorBasic::loadOperand(std::string reg, MxIR::Operand src)
{
	if (src.isReg())
		writeCode("mov ", reg, ", ", getVReg(src));
	else if (src.isConst())
		writeCode("mov ", reg, ", ", getConst(src));
	else
	{
		assert(false);
	}
}

std::string CodeGeneratorBasic::storeOperand(MxIR::Operand dst, int id)
{
	std::string name = regName(id, dst.size());
	assert(dst.isReg());
	writeCode("mov ", getVReg(dst), ", ", name);
	return name;
}

std::string CodeGeneratorBasic::getConst(MxIR::Operand src, bool immSigned, int tempreg)
{
	if (src.isImm())
	{
		if (tempreg > 0 && src.type == Operand::imm64 && (std::int64_t(src.val) > INT_MAX || std::int64_t(src.val) < INT_MIN))
		{
			writeCode("mov ", regName(tempreg, 8), ", ", src.val);
			return regName(tempreg, 8);
		}
		std::stringstream ss;
		if (immSigned)
		{
			if (src.type == Operand::imm8)
				ss << int(std::int8_t(src.val));
			else if (src.type == Operand::imm16)
				ss << std::int16_t(src.val);
			else if (src.type == Operand::imm32)
				ss << std::int32_t(src.val);
			else if (src.type == Operand::imm64)
				ss << std::int64_t(src.val);
			else
			{
				assert(false);
			}
		}
		else
		{
			if (src.type == Operand::imm8)
				ss << int(std::uint8_t(src.val));
			else if (src.type == Operand::imm16)
				ss << std::uint16_t(src.val);
			else if (src.type == Operand::imm32)
				ss << std::uint32_t(src.val);
			else if (src.val == Operand::imm64)
				ss << std::uint64_t(src.val);
			else
			{
				assert(false);
			}
		}
		return ss.str();
	}
	if (src.type == Operand::funcID)
		return labelFunc[src.val];
	if (src.type == Operand::globalVarID)
		return labelVar[src.val];
	if (src.type == Operand::externalSymbolName)
		return symbol->vSymbol[src.val];
	if (src.type == Operand::constID)
	{
		std::stringstream ss;
		ss << "_C_" << src.val;
		return ss.str();
	}
	assert(false);
	return "";
}

std::string CodeGeneratorBasic::getVReg(MxIR::Operand src)
{
	return sizeName(src.size()) + " " + getVRegAddr(src);
}

std::string CodeGeneratorBasic::getVRegAddr(MxIR::Operand src)
{
	assert(src.isReg());
	std::stringstream ss;
	if (regAddr[src.val] > 0)
		ss << "[rbp-" << regAddr[src.val] << "]";
	else
		ss << "[rbp+" << -regAddr[src.val] << "]";
	return ss.str();
}


void CodeGeneratorBasic::translateIns(Instruction ins)
{
	static const std::unordered_map<int, std::string> mapInsBinary = {
		{Add, "add"}, {Sub, "sub"}, {Mult, "imul"},
		//
		{And, "and"}, {Or, "or"}, {Xor, "xor"}};
	static const std::unordered_map<int, std::string> mapInsUnary = {
		{Neg, "neg"}, {Not, "not"}};

	auto iterBinary = mapInsBinary.find(ins.oper);
	if (iterBinary != mapInsBinary.end())
	{
		if (ins.src2.isConst())
		{
			std::string tmpReg = loadOperand(0, ins.src1);
			writeCode(iterBinary->second, " ", tmpReg, ", ", getConst(ins.src2));
			storeOperand(ins.dst, 0);
		}
		else
		{
			std::string src1 = loadOperand(0, ins.src1);
			//std::string src2 = loadOperand(1, ins.src2);
			writeCode(iterBinary->second, " ", src1, ", ", getVReg(ins.src2));
			storeOperand(ins.dst, 0);
		}
		return;
	}
	auto iterUnary = mapInsUnary.find(ins.oper);
	if (iterUnary != mapInsUnary.end())
	{
		std::string src = loadOperand(0, ins.src1);
		writeCode(iterUnary->second, " ", src);
		storeOperand(ins.dst, 0);
		return;
	}

	static const std::unordered_map<int, std::string> mapInsShift = {
		{ Shl, "sal" }, { Shr, "sar" },
		{ Shlu, "shl" }, { Shru, "shr"}, };
	auto iterShift = mapInsShift.find(ins.oper);
	if (iterShift != mapInsShift.end())
	{
		if (ins.src2.isConst())
		{
			std::string tmpReg = loadOperand(0, ins.src1);
			writeCode(iterShift->second, " ", tmpReg, ", ", getConst(ins.src2));
			storeOperand(ins.dst, 0);
		}
		else
		{
			std::string tmpReg = loadOperand(0, ins.src1);
			loadOperand(1, ins.src2);
			writeCode(iterShift->second, " ", tmpReg, ", cl");
			storeOperand(ins.dst, 0);
		}
		return;
	}

	if (ins.oper == Div || ins.oper == Mod)
	{
		std::string src1 = loadOperand(0, ins.src1);
		std::string src2 = loadOperand(1, ins.src2);
		assert(ins.src1.size() == ins.src2.size());
		if (ins.src1.size() == 8)
			writeCode("cqo");
		else if (ins.src1.size() == 4)
			writeCode("cdq");
		else if (ins.src1.size() == 2)
			writeCode("cwd");
		writeCode("idiv ", src2);
		if (ins.oper == Div)
			storeOperand(ins.dst, 0);	//quotient in rax
		else
			storeOperand(ins.dst, 2);	//remainder in rdx
		return;
	}
	if (ins.oper == Move)
	{
		if (ins.src1.isConst())
			writeCode("mov ", getVReg(ins.dst), ", ", getConst(ins.src1, true, -1));
		else
		{
			loadOperand(0, ins.src1);
			storeOperand(ins.dst, 0);
		}
		return;
	}
	if (ins.oper == Load || ins.oper == Store)
	{
		std::string addr = ins.src1.isConst() ? getConst(ins.src1) : loadOperand(0, ins.src1);

		if (ins.oper == Load)
		{
			writeCode("mov ", regName(1, ins.dst.size()), ", ", sizeName(ins.dst.size()), " [", addr, "]");
			storeOperand(ins.dst, 1);
		}
		else
		{
			std::string data = ins.dst.isConst() ? getConst(ins.dst) : loadOperand(1, ins.dst);
			writeCode("mov ", sizeName(ins.dst.size()), " [", addr, "], ", data);
		}
		return;
	}
	if (ins.oper == LoadA || ins.oper == StoreA)
	{
		std::string addr = ins.src1.isConst() && !ins.src2.isConst() ? getConst(ins.src1) : loadOperand(0, ins.src1);
		std::string offset = ins.src2.isConst() ? getConst(ins.src2) : loadOperand(1, ins.src2);
		//writeCode("lea ", regName(2, 8), ", [", addr, " + ", offset, "]");

		if (ins.oper == LoadA)
		{
			writeCode("mov ", regName(2, ins.dst.size()), ", ", sizeName(ins.dst.size()), " [", addr, "+", offset, "]");
			storeOperand(ins.dst, 2);
		}
		else
		{
			std::string data = ins.dst.isConst() ? getConst(ins.dst) : loadOperand(2, ins.dst);
			writeCode("mov ", sizeName(ins.dst.size()), " [", addr, "+", offset, "], ", data);
		}
		return;
	}
	if (ins.oper == Sext || ins.oper == Zext)
	{
		if (ins.src1.isImm())
		{
			writeCode("mov ", regName(0, ins.dst.size()), ", ", getConst(ins.src1, ins.oper == Sext));
			storeOperand(ins.dst, 0);
			return;
		}

		if (ins.oper == Zext && ins.src1.size() >= 4)
		{
			writeCode("mov ", regName(0, ins.src1.size()), ", ", getVReg(ins.src1));
			/**********************************************************
			  the upper 32bit of the target register will be zeroed, see:
			  http://www.intel.com/Assets/en_US/PDF/manual/253665.pdf
			  3.4.1.1 General-Purpose Registers in 64-Bit Mode in manual Basic Architecture
			  "32-bit operands generate a 32-bit result, zero-extended to a 64-bit result in the destination general - purpose register"
			***********************************************************/
			storeOperand(ins.dst, 0);
		}
		else
		{
			std::string op = ins.oper == Sext ? "movsx " : "movzx ";
			writeCode(op, regName(0, ins.dst.size()), ", ", getVReg(ins.src1));
			storeOperand(ins.dst, 0);
		}
		return;
	}
	if (ins.oper == Call)
	{
		for (size_t i = 0; i < 6 && i < ins.paramExt.size(); i++)
			loadOperand(paramRegID[i], ins.paramExt[i]);

		size_t rspOffset = 0;
		if (ins.paramExt.size() > 6 && ins.paramExt.size() % 2 == 1)
		{
			writeCode("sub rsp, 8");
			rspOffset += 8;
		}
		for (ssize_t i = ins.paramExt.size() - 1; i >= 6; i--)
		{
			loadOperand(0, ins.paramExt[i]);
			writeCode("push ", regName(0, 8));
			rspOffset += 8;
		}
		writeCode("call ", getConst(ins.src1));
		if(rspOffset > 0)
			writeCode("add rsp, ", rspOffset);
		if (ins.dst.type != Operand::empty)
			storeOperand(ins.dst, 0);
		return;
	}
	if (ins.oper == Allocate)
	{
		for (auto iter = allocAddr.begin(); iter != allocAddr.end(); ++iter)
		{
			size_t size, align;
			std::int64_t addr;
			std::tie(size, align, addr) = *iter;
			if (size == ins.src1.val && align == ins.src2.val)
			{
				writeCode("lea ", regName(0, 8), ", [rbp-(", addr, ")]");
				storeOperand(ins.dst, 0);
				allocAddr.erase(iter);
				return;
			}
		}
		assert(false);
		return;
	}
	if (ins.oper == Return)
	{
		if (ins.src1.type != Operand::empty)
			loadOperand(0, ins.src1);
		else
			writeCode("xor rax, rax");
		writeCode("mov rsp, rbp");
		writeCode("pop rbp");
		writeCode("ret");
		return;
	}
	if (ins.oper == Br)
	{
		std::string tempReg = loadOperand(0, ins.src1);	//FIXME: src1 may be imm
		writeCode("test ", tempReg, ", ", tempReg);
		return;
	}

	static const std::unordered_map<int, std::string> mapInsCmp = {
		{Slt, "setl"}, {Sle, "setle"}, {Seq, "sete"}, {Sne, "setne"}, {Sgt, "setg"}, {Sge, "setge"},
		{Sltu, "setb"}, {Sleu, "setbe"}, {Sgtu, "seta"}, {Sgeu, "setae"}};
	static const std::unordered_map<int, std::string> mapInsCmpRev = {
		{ Sge, "setl" },{ Sgt, "setle" },{ Seq, "sete" },{ Sne, "setne" },{ Sle, "setg" },{ Slt, "setge" },
		{ Sgeu, "setb" },{ Sgtu, "setbe" },{ Sleu, "seta" },{ Sltu, "setae" } };

	std::string opl = ins.src1.isConst() && !ins.src2.isConst() ? getConst(ins.src1) : loadOperand(0, ins.src1);
	std::string opr = ins.src2.isConst() ? getConst(ins.src2) : loadOperand(1, ins.src2);
	if (ins.src1.isConst() && !ins.src2.isConst())
	{
		writeCode("cmp ", opr, ", ", opl);
		writeCode(mapInsCmpRev.find(ins.oper)->second, " ", regName(2, 1));
	}
	else
	{
		writeCode("cmp ", opl, ", ", opr);
		writeCode(mapInsCmp.find(ins.oper)->second, " ", regName(2, 1));
	}
	if (ins.dst.size() > 1)
		writeCode("movzx ", regName(2, ins.dst.size()), ", ", regName(2, 1));
	storeOperand(ins.dst, 2);
}

void CodeGeneratorBasic::generateProgram()
{
	std::set<size_t> externSymbol, exportSymbol;
	auto checkExternalSymbol = [&externSymbol](Operand operand)
	{
		if (operand.type == Operand::externalSymbolName)
			externSymbol.insert(operand.val);
	};
	for (auto &finfo : program->vFuncs)
	{
		assert(finfo.content.inBlock);
		finfo.content.inBlock->traverse([&](Block *blk)
		{
			for (auto &ins : blk->ins)
			{
				checkExternalSymbol(ins.dst);
				checkExternalSymbol(ins.src1);
				checkExternalSymbol(ins.src2);
				for (auto &p : ins.paramExt)
					checkExternalSymbol(p);
			}
			return true;
		});
		if (finfo.attribute & Export)
			exportSymbol.insert(finfo.funcName);
	}

	if (!externSymbol.empty())
	{
		out << "extern " << symbol->vSymbol[*externSymbol.begin()];
		for (auto iter = ++externSymbol.begin(); iter != externSymbol.end(); ++iter)
			out << ", " << symbol->vSymbol[*iter];
		out << std::endl;
	}
	if (!exportSymbol.empty())
	{
		out << "global " << symbol->vSymbol[*exportSymbol.begin()];
		for (auto iter = ++exportSymbol.begin(); iter != exportSymbol.end(); ++iter)
			out << ", " << symbol->vSymbol[*iter];
		out << std::endl;
	}

	createLabel();
	cntLocalLabel = 0;

	writeCode("section .text");
	for (size_t i = 0; i < program->vFuncs.size(); i++)
	{
		if (program->vFuncs[i].disabled)
			continue;
		generateFunc(program->vFuncs[i], labelFunc[i]);
		out << std::endl;
	}
	for (size_t i = 0; i < program->vConst.size(); i++)
	{
		std::stringstream ss;
		ss << "_C_" << i;
		generateConst(program->vConst[i], ss.str());
		out << std::endl;
	}

	writeCode("section .bss");
	writeCode("align 16");
	for (size_t i = 0; i < program->vGlobalVars.size(); i++)
	{
		if (program->vGlobalVars[i].varType.mainType == MxType::Function)
			continue;
		generateVar(program->vGlobalVars[i], labelVar[i]);
		out << std::endl;
	}
}