#ifndef MX_COMPILER_INLINE_OPTIMIZER_H
#define MX_COMPILER_INLINE_OPTIMIZER_H

#include "common.h"
#include "MxProgram.h"

namespace MxIR
{
	class InlineOptimizer
	{
	public:
		InlineOptimizer() : program(MxProgram::getDefault()) {}
		InlineOptimizer(MxProgram *program) : program(program) {}
		void work();

	protected:
		struct statFunc
		{
			size_t nInsn, nBlock;
			size_t nVar;
			bool externalCall, forceInline;
			std::map<size_t, size_t> callTo;	//func ID -> call times

			size_t nUpdate;

			statFunc() : nInsn(0), nBlock(0), nVar(0), externalCall(false), nUpdate(0) {}
			size_t penalty() const;
		};

	protected:
		void analyzeProgram();
		statFunc analyzeFunc(Function &func);
		void applyInline(size_t callee, size_t caller, Function &content);

	protected:
		std::vector<statFunc> stats;
		MxProgram *program;
	};
}

#endif