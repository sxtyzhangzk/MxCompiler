#include "common_headers.h"
#include "LoopDetector.h"

namespace MxIR
{
	void LoopDetector::findLoops()
	{
		func.inBlock->traverse([this](Block *block) -> bool
		{
			mapBlock[block] = vBlock.size();
			vBlock.push_back(block);
			return true;
		});
		dtree = DomTree(vBlock.size());
		for (Block *blk : vBlock)
		{
			if (blk->brTrue)
				dtree.link(mapBlock[blk], mapBlock[blk->brTrue.get()]);
			if (blk->brFalse)
				dtree.link(mapBlock[blk], mapBlock[blk->brFalse.get()]);
		}
		assert(vBlock[0] == func.inBlock.get());
		dtree.buildTree(0);

		std::set<Block *> predecessors;
		dfs_dtree(0, predecessors);
	}

	void LoopDetector::dfs_dtree(size_t idx, std::set<Block *> &predecessors)
	{
		Block *blk = vBlock[idx];
		for (auto *child : { &blk->brTrue, &blk->brFalse })
			if (*child && predecessors.count(child->get()))
				dfs_backward(blk, child->get());
		predecessors.insert(blk);
		for (size_t child_dtree : dtree.getDomChildren(idx))
			dfs_dtree(child_dtree, predecessors);
		predecessors.erase(blk);
	}

	void LoopDetector::dfs_backward(Block *blk, Block *target)
	{
		loops[target].insert(blk);
		if (blk == target)
			return;
		for (Block *pred : blk->preds)
			if (!loops[target].count(pred))
				dfs_backward(pred, target);
	}
}