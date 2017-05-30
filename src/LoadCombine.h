#ifndef MX_COMPILER_LOAD_COMBINE_H
#define MX_COMPILER_LOAD_COMBINE_H

#include "common.h"
#include "IR.h"

namespace MxIR
{
	class LoadCombine
	{
	public:
		LoadCombine(Function &func) : func(func) {}
		void work();

	protected:
		static void combine(Block *block);

	protected:
		Function &func;
	};
}

#endif