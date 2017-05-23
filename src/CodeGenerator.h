#ifndef MX_COMPILER_CODE_GENERATOR_H
#define MX_COMPILER_CODE_GENERATOR_H

#include "common.h"
#include "CodeGeneratorBasic.h"

class CodeGenerator : public CodeGeneratorBasic
{
public:
	CodeGenerator(std::ostream &out) : CodeGeneratorBasic(out) {}

protected:
	virtual void generateFunc(MxProgram::funcInfo &finfo, const std::string &label) override;
	virtual void translateIns(MxIR::Instruction ins) override; 

	void initFuncEntryExit();
	void regularizeInsnPre();
	void setRegisterConstrains();
	void setRegisterPrefer();
	void regularizeInsnPost();
	void allocateStackFrame();
	std::string getOperand(MxIR::Operand operand);

protected:
	bool popRBP;
	size_t varID;
	MxIR::Function *func;
	std::map<size_t, ssize_t> stackFrame;
	size_t stackSize;
	static const std::vector<int> regCallerSave, regCalleeSave, regParam;
};

#endif