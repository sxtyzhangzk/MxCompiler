#ifndef MX_COMPILER_INSTRUCTION_SELECT_H
#define MX_COMPILER_INSTRUCTION_SELECT_H

#include "common.h"
#include "IR.h"

namespace MxIR
{
	class InstructionSelect
	{
	public:
		InstructionSelect(Function &func) : func(func) {}
		void work();

	protected:
		void computeUseCount();
		void selectInsn(Block *block);

	protected:
		Function &func;
		std::map<Operand, size_t> useCount;
	};
}

#endif