#ifndef MX_COMPILER_LOOP_DETECTOR
#define MX_COMPILER_LOOP_DETECTOR

#include "common.h"
#include "IR.h"
#include "utils/DomTree.h"

namespace MxIR
{
	class LoopDetector
	{
	public:
		LoopDetector(Function &func) : func(func) {}
		void findLoops();
		const std::map<Block *, std::set<Block *>> &getLoops() const { return loops; }

	protected:
		void dfs_dtree(size_t idx, std::set<Block *> &predecessors);
		void dfs_backward(Block *blk, Block *target);

	protected:
		Function &func;

		std::map<Block *, size_t> mapBlock;
		std::vector<Block *> vBlock;
		DomTree dtree;

		std::map<Block *, std::set<Block *>> loops;	//loop header -> set of loop body
	};
}

#endif