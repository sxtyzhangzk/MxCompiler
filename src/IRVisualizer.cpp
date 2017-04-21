#include "IRVisualizer.h"
#include <sstream>
#include <unordered_map>
#include <vector>
using namespace MxIR;

std::string IRVisualizer::toString(const Operand &operand)
{
	std::stringstream ss;
	switch (operand.type)
	{
	case Operand::empty:
		return "";
	case Operand::imm8: case Operand::imm16: case Operand::imm32: case Operand::imm64:
		ss << operand.val;
		return ss.str();
	case Operand::reg8: case Operand::reg16: case Operand::reg32: case Operand::reg64:
		ss << "%" << operand.val;
		return ss.str();
	case Operand::funcID:
		ss << "func " << operand.val << " [" << symbol->vSymbol[program->vFuncs[operand.val].funcName] << "]";
		return ss.str();
	case Operand::constID:
		ss << "const " << operand.val;
		return ss.str();
	case Operand::globalVarID:
		ss << "gvar " << operand.val << " [" << symbol->vSymbol[program->vGlobalVars[operand.val].varName] << "]";
		return ss.str();
	case Operand::externalSymbolName:
		ss << "extern [" << symbol->vSymbol[operand.val] << "]";
		return ss.str();
	default:
		assert(false);
	}
	return "";
}

std::string IRVisualizer::toString(const Instruction &ins)
{
	static const std::unordered_map<int, std::string> mapOper = {
		{Add, "+"}, {Sub, "-"}, {Mult, "*"}, {Div, "/"}, {Mod, "%"},
		{Shl, "<<"}, {Shr, ">>"}, {And, "&"}, {Or, "|"}, {Xor, "^"},
		{Neg, "-"}, {Not, "~"}, {Sext, "sext"}, {Zext, "zext"},
		{Slt, "<"}, {Sle, "<="}, {Sgt, ">"}, {Sge, ">="}, {Seq, "=="}, {Sne, "!="},
		{Sltu, "<"}, {Sleu, "<="}, {Sgtu, ">"}, {Sgeu, ">="},
	};
	std::stringstream ss;
	switch (ins.oper)
	{
	case Nop:
		return "nop";
	case Add: case Sub: case Mult: case Div: case Mod: case Shl: case Shr: case And: case Or: case Xor:
		ss << toString(ins.dst) << " = " << toString(ins.src1) << " " << mapOper.find(ins.oper)->second << " " << toString(ins.src2);
		return ss.str();
	case Neg: case Not: case Sext: case Zext:
		ss << toString(ins.dst) << " = " << mapOper.find(ins.oper)->second << " " << toString(ins.src1);
		return ss.str();
	case Load:
		ss << toString(ins.dst) << " = load " << toString(ins.src1);
		return ss.str();
	case LoadA:
		ss << toString(ins.dst) << " = load (" << toString(ins.src1) << ", " << toString(ins.src2) << ")";
		return ss.str();
	case Store:
		ss << "store " << toString(ins.dst) << " to " << toString(ins.src1);
		return ss.str();
	case StoreA:
		ss << "store " << toString(ins.dst) << " to (" << toString(ins.src1) << ", " << toString(ins.src2) << ")";
		return ss.str();
	case Move:
		ss << toString(ins.dst) << " = " << toString(ins.src1);
		return ss.str();
	case Slt: case Sle: case Sgt: case Sge: case Seq: case Sne:
		ss << toString(ins.dst) << " = cmp " << toString(ins.src1) << " " << mapOper.find(ins.oper)->second << " " << toString(ins.src2);
		return ss.str();
	case Sltu: case Sleu: case Sgtu: case Sgeu:
		ss << toString(ins.dst) << " = cmp unsigned " << toString(ins.src1) << " " << mapOper.find(ins.oper)->second << " " << toString(ins.src2);
		return ss.str();
	case Br:
		ss << "br " << toString(ins.src1) << " " << (ins.src2.val == likely ? "likely" : "") << (ins.src2.val == unlikely ? "unlikely" : "");
		return ss.str();
	case Jump:
		return "jmp";
	case Call: 
		ss << (ins.dst.type == Operand::empty ? "" : toString(ins.dst) + " = ") << "call " << toString(ins.src1);
		ss << " (";
		for (size_t i = 0; i < ins.paramExt.size(); i++)
			ss << toString(ins.paramExt[i]) << (i < ins.paramExt.size() - 1 ? ", " : "");
		ss << ")";
		return ss.str();
	case Return:
		ss << "ret" << (ins.src1.type == Operand::empty ? "" : " " + toString(ins.src1));
		return ss.str();
	case Allocate:
		ss << toString(ins.dst) << " = allocate " << toString(ins.src1) << ", align " << toString(ins.src2);
		return ss.str();
	default:
		assert(false);
	}
	return "";
}

std::string IRVisualizer::toString(const Block &block)
{
	std::string ret;
	for (auto &ins : block.ins)
		ret += toString(ins) + "\n";
	return ret;
}

std::string IRVisualizer::toHTML(const Block &block, int flag, const std::string &funcName)
{
	std::string text = transferHTML(toString(block));
	std::string ret = "<table border='0' cellborder='1' cellspacing='0'>";
	std::string colspan;
	if (block.brTrue && block.brFalse)
		colspan = "colspan='2'";
	if (flag == 1)
	{
		ret += "<tr><td " + colspan + " align='center'>BEGIN " + transferHTML(funcName) + "</td></tr>";
		ret += "<tr><td " + colspan + " align='left'><font face='Consolas'> ";
		ret += text;
		ret += "</font></td></tr>";
	}
	else if (flag == 2)
	{
		assert(!block.brTrue && !block.brFalse);
		ret += "<tr><td " + colspan + " align='left'><font face='Consolas'> ";
		ret += text;
		ret += "</font></td></tr>";
		ret += "<tr><td " + colspan + " align='center'>END " + transferHTML(funcName) + "</td></tr>";
	}
	else
	{
		ret += "<tr><td " + colspan + " align='left'><font face='Consolas'> ";
		ret += text;
		ret += "</font></td></tr>";
	}
	if (block.brTrue || block.brFalse)
	{
		ret += "<tr>";
		if (block.brTrue)
			ret += "<td port='btrue'>branch true</td>";
		if (block.brFalse)
			ret += "<td port='bfalse'>branch false</td>";
		ret += "</tr>";
	}
	ret += "</table>";
	return ret;
}

void IRVisualizer::print(const Function &func, const std::string &funcName)
{
	if (!func.inBlock)
		return;
	std::unordered_map<Block *, size_t> mapID;
	std::vector<Block *> vBlock;
	func.inBlock->traverse([&](Block *block) -> bool
	{
		mapID.insert({ block, vBlock.size() });
		vBlock.push_back(block);
		return true;
	});
	for (size_t i = 0; i < vBlock.size(); i++)
	{
		int flag = 0;
		if (vBlock[i] == func.inBlock.get())
			flag = 1;
		else if (vBlock[i] == func.outBlock.get())
			flag = 2;
		out << i + cntBlock << " [shape=none,label=<" << toHTML(*vBlock[i], flag, funcName) << ">];" << std::endl;
		if (vBlock[i]->brTrue)
			out << i + cntBlock << ":btrue -> " << mapID.find(vBlock[i]->brTrue.get())->second + cntBlock << ";" << std::endl;
		if (vBlock[i]->brFalse)
			out << i + cntBlock << ":bfalse -> " << mapID.find(vBlock[i]->brFalse.get())->second + cntBlock << ";" << std::endl;
	}
	cntBlock += vBlock.size();
}

void IRVisualizer::printAll()
{
	printHead();
	for (auto &finfo : program->vFuncs)
	{
		if (finfo.isThiscall)
		{
			std::string className = "<unknown>";
			if (finfo.paramType[0].mainType == MxType::String)
				className = "<string>";
			else if (finfo.paramType[0].arrayDim > 0)
				className = "<array>";
			else if (finfo.paramType[0].className != size_t(-1))
				className = symbol->vSymbol[finfo.paramType[0].className];
			print(finfo.content, className + "::" + symbol->vSymbol[finfo.funcName]);
		}
		else
			print(finfo.content, symbol->vSymbol[finfo.funcName]);
	}
	printFoot();
}