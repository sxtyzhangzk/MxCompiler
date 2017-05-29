#include "common_headers.h"
#include "DeadCodeElimination.h"

namespace MxIR
{
	void DeadCodeElimination::work()
	{
		static const size_t maxIter = 50;

		func.splitProgramRegion();
		func.constructPST();

		collectVars();
		
		size_t i = 0;
		do
		{
			eliminateVars();
			countGlobalUse();

			regionUpdated = false;
			eliminateRegions(func.pstRoot.get());
		} while (regionUpdated && ++i < maxIter);
	}

	bool DeadCodeElimination::hasSideEffect(const Instruction &ins)
	{
		return ins.oper == Call && (ins.src1.type != Operand::funcID || !(program->vFuncs[ins.src1.val].attribute & NoSideEffect))
			|| ins.oper == Store || ins.oper == StoreA
			|| ins.oper == Return;
	}

	void DeadCodeElimination::collectVars()
	{
		func.inBlock->traverse([this](Block *block) -> bool
		{
			for (auto iter = block->phi.begin(); iter != block->phi.end(); ++iter)
			{
				vars[iter->second.dst].block = block;
				vars[iter->second.dst].isPhi = true;
				vars[iter->second.dst].iterPhi = iter;
			}
			for (auto iter = block->ins.begin(); iter != block->ins.end(); ++iter)
			{
				for (Operand *operand : iter->getOutputReg())
				{
					vars[*operand].block = block;
					vars[*operand].isPhi = false;
					vars[*operand].iterInsn = iter;
				}
			}
			return true;
		});
	}

	void DeadCodeElimination::eliminateVars()
	{
		std::set<Operand> marked;
		func.inBlock->traverse([&marked, this](Block *block) -> bool
		{
			bool flag = false;
			for (auto &ins : block->ins)
			{
				if (hasSideEffect(ins) || ins.oper == Br)
				{
					for (Operand *operand : join<Operand *>(ins.getInputReg(), ins.getOutputReg()))
						marked.insert(*operand);
					if(ins.oper != Br)
						flag = true;
				}
			}
			if (flag)
				blockBlacklist.insert(block);
			return true;
		});

		std::queue<Operand> worklist;
		for (auto &op : marked)
			worklist.push(op);
		while (!worklist.empty())
		{
			Operand cur = worklist.front();
			worklist.pop();

			if (!vars.count(cur))
				continue;

			if (vars[cur].isPhi)
			{
				for (auto &src : vars[cur].iterPhi->second.srcs)
				{
					if (src.first.isReg())
						if (!marked.count(src.first))
						{
							marked.insert(src.first);
							worklist.push(src.first);
						}
				}
			}
			else
			{
				for (Operand *operand : vars[cur].iterInsn->getInputReg())
				{
					if (!marked.count(*operand))
					{
						marked.insert(*operand);
						worklist.push(*operand);
					}
				}
			}
		}

		func.inBlock->traverse([&marked, this](Block *block) -> bool
		{
			for (auto iter = block->phi.begin(); iter != block->phi.end();)
			{
				if (!marked.count(iter->second.dst))
					iter = block->phi.erase(iter);
				else
					++iter;
			}
			for (auto iter = block->ins.begin(); iter != block->ins.end();)
			{
				if (hasSideEffect(*iter) || iter->oper == Jump || iter->oper == Br)
				{
					++iter;
					continue;
				}

				bool flag = false;
				for (Operand *operand : iter->getOutputReg())
					if (marked.count(*operand))
					{
						flag = true;
						break;
					}
				if (!flag)
					iter = block->ins.erase(iter);
				else
					++iter;
			}
			return true;
		});
	}

	void DeadCodeElimination::countGlobalUse()
	{
		for (auto &kv : vars)
			kv.second.useCount = 0;
		func.inBlock->traverse([this](Block *block) -> bool
		{
			for (auto &ins : block->instructions())
				for (Operand *operand : ins.getInputReg())
				{
					if (!vars.count(*operand))
						continue;
					vars[*operand].useCount++;
				}
			return true;
		});
	}

	std::map<Operand, size_t> DeadCodeElimination::countUse(const std::set<Block *> &blocks)
	{
		std::map<Operand, size_t> ret;
		for (Block *block : blocks)
		{
			for (auto &ins : block->instructions())
				for (Operand *operand : ins.getInputReg())
				{
					if (!vars.count(*operand))
						continue;
					ret[*operand]++;
				}
		}
		return ret;
	}

	std::set<Operand> DeadCodeElimination::findDef(const std::set<Block *> &blocks)
	{
		std::set<Operand> ret;
		for (Block *block : blocks)
		{
			for (auto &ins : block->instructions())
				for (Operand *operand : ins.getOutputReg())
					ret.insert(*operand);
		}
		return ret;
	}

	int DeadCodeElimination::eliminateRegions(PSTNode *node)
	{
		bool blacklisted = false;
		for (auto iter = node->children.begin(); iter != node->children.end();)
		{
			int ret = eliminateRegions(iter->get());
			if (ret == 1)
				iter = node->children.erase(iter);
			else
				++iter;

			if (ret == -1)
				blacklisted = true;
		}

		if (blacklisted)
			return -1;

		for (Block *block : node->blocks)
			if (blockBlacklist.count(block))
				return -1;

		if (node->inBlock == func.inBlock.get() || node->outBlock == func.outBlock.get())
			return -1;

		std::set<Block *> blocks = node->getBlocks();

		auto def = findDef(blocks);
		auto use = countUse(blocks);
		for (auto &op : def)
		{
			assert(vars.count(op));
			if (vars[op].useCount != use[op])
				return 0;
		}

		

		/*std::cerr << "eliminate region with " << blocks.size() << " blocks" << std::endl;
		IRVisualizer visual(std::cerr);
		std::cerr << visual.toString(**blocks.begin(), false) << std::endl;*/

		Block *pred = nullptr, *next = nullptr;
		for (Block *block : node->inBlock->preds)
			if (!blocks.count(block))
			{
				assert(!pred);
				pred = block;
			}
		for (Block *block : { node->outBlock->brTrue.get(), node->outBlock->brFalse.get() })
			if (block && !blocks.count(block))
			{
				assert(!next);
				next = block;
			}

		std::shared_ptr<Block> inBlock = node->inBlock->self.lock();

		bool flag = false;
		std::shared_ptr<Block> tmp;
		if (pred->brTrue.get() == next || pred->brFalse.get() == next)
		{
			if (blocks.size() == 1 && (*blocks.begin())->ins.size() == 1 && (*blocks.begin())->ins.front().oper == Jump)
				flag = true;

			tmp = Block::construct();
			tmp->ins = { IRJump() };
			if (inBlock.get() == pred->brTrue.get())
				pred->brTrue = tmp;
			if (inBlock.get() == pred->brFalse.get())
				pred->brFalse = tmp;
			tmp->brTrue = inBlock;
			pred = tmp.get();
		}
		
		if (inBlock.get() == pred->brTrue.get())
			pred->brTrue = next->self.lock();
		if (inBlock.get() == pred->brFalse.get())
			pred->brFalse = next->self.lock();
		next->redirectPhiSrc(node->outBlock, pred);

		if (node->outBlock->brTrue.get() == next)
			node->outBlock->brTrue.reset();
		else
			node->outBlock->brFalse.reset();

		if(!flag)
			regionUpdated = true;
		
		return 1;
	}
}