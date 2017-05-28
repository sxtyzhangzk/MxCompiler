#ifndef MX_COMPILER_DEAD_CODE_ELIMINATION_H
#define MX_COMPILER_DEAD_CODE_ELIMINATION_H

#include "common.h"
#include "MxProgram.h"
#include "IR.h"

namespace MxIR
{
	class DeadCodeElimination
	{
	public:
		DeadCodeElimination(Function &func) : func(func), program(MxProgram::getDefault()) {}
		void work();

	protected:
		bool hasSideEffect(const Instruction &ins);
		void collectVars();
		void eliminateVars();

		void countGlobalUse();
		std::map<Operand, size_t> countUse(const std::set<Block *> &blocks);
		static std::set<Operand> findDef(const std::set<Block *> &blocks);
		int eliminateRegions(PSTNode *node);

	protected:
		struct VarProperty
		{
			bool isPhi;
			Block *block;
			std::list<Instruction>::iterator iterInsn;
			std::map<size_t, Block::PhiIns>::iterator iterPhi;

			size_t useCount = 0;
		};

		MxProgram *program;
		Function &func;
		std::map<Operand, VarProperty> vars;
		std::set<Block *> blockBlacklist;
		bool regionUpdated;
	};
}

#endif