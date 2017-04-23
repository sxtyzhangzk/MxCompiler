#ifndef MX_COMPILER_CODE_GENERATOR_BASIC_H
#define MX_COMPILER_CODE_GENERATOR_BASIC_H

#include "common.h"
#include "IR.h"
#include "MxProgram.h"
#include "GlobalSymbol.h"

class CodeGeneratorBasic
{
public:
	CodeGeneratorBasic(std::ostream &out) : 
		program(MxProgram::getDefault()), symbol(GlobalSymbol::getDefault()), out(out), cntLocalLabel(0) {}

	void generateProgram();

protected:
	void createLabel();
	std::string decorateFuncName(const MxProgram::funcInfo &finfo);
	void generateFunc(const MxProgram::funcInfo &finfo, const std::string &label);
	void generateConst(const MxProgram::constInfo &cinfo, const std::string &label);
	void generateVar(const MxProgram::varInfo &vinfo, const std::string &label);


	std::vector<MxIR::Block *> sortBlocks(MxIR::Block *inBlock);
	void translateBlocks(const std::vector<MxIR::Block *> &vBlocks);
	void translateIns(MxIR::Instruction ins);
	std::string loadOperand(int id, MxIR::Operand src);
	void loadOperand(std::string reg, MxIR::Operand src);
	std::string getConst(MxIR::Operand src, bool immSigned = true, int tempreg = 11);
	std::string getVReg(MxIR::Operand src);
	std::string getVRegAddr(MxIR::Operand src);
	std::string storeOperand(MxIR::Operand dst, int id);


	static std::string regName(int id, size_t size);
	static std::string sizeName(size_t size);

	template<typename ...T>
	void writeCode(T&&... val)
	{
		out << "\t";
		prints(out, std::forward<T>(val)...);
		out << std::endl;
	}
	template<typename ...T>
	void writeLabel(T&&... val)
	{
		prints(out, std::forward<T>(val)...);
		out << ":" << std::endl;
	}

protected:
	MxProgram *program;
	GlobalSymbol *symbol;
	std::ostream &out;
	std::vector<std::string> labelFunc, labelVar;
	size_t cntLocalLabel;
	std::vector<std::int64_t> regAddr;
	std::list<std::tuple<size_t, size_t, std::int64_t>> allocAddr;

	static const std::string paramReg[];
	static const int paramRegID[];
};

#endif