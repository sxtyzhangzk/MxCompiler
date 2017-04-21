#ifndef MX_COMPILER_IR_VISUALIZER_H
#define MX_COMPILER_IR_VISUALIZER_H

#include "IR.h"
#include "GlobalSymbol.h"
#include "MxProgram.h"
#include <string>
#include <iostream>

class IRVisualizer
{
public:
	IRVisualizer(std::ostream &out) : out(out), symbol(GlobalSymbol::getDefault()), program(MxProgram::getDefault()), cntBlock(0) {}
	std::string toString(const MxIR::Operand &operand);
	std::string toString(const MxIR::Instruction &ins);
	std::string toString(const MxIR::Block &block);
	std::string toHTML(const MxIR::Block &block, int flag, const std::string &funcName);	//flag: 1 for in block, 2 for out block
	void print(const MxIR::Function &func, const std::string &funcName);
	void printHead() { out << "digraph mxprog {" << std::endl; }
	void printFoot() { out << "}" << std::endl; }
	void reset() { cntBlock = 0; }

	void printAll();

protected:
	std::ostream &out;
	GlobalSymbol *symbol;
	MxProgram *program;
	bool enableColor;
	size_t cntBlock;
};

#endif