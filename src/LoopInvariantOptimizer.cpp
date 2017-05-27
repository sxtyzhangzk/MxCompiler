#include "common_headers.h"
#include "LoopInvariantOptimizer.h"
#include "LoopDetector.h"

namespace MxIR
{
	void LoopInvariantOptimizer::work()
	{
		func.splitProgramRegion();
		func.constructPST();

		computeMaxVer();

		LoopDetector detector(func);
		detector.findLoops();
		for (auto &kv : detector.getLoops())
		{
			loop cur;
			cur.header = kv.first;
			cur.body = kv.second;
			
			loops.push_back(cur);
		}
		std::sort(loops.begin(), loops.end(), [](const loop &a, const loop &b) { return a.body.size() > b.body.size(); });

		for (size_t i = 0; i < loops.size(); i++)
		{
			for (Block *block : loops[i].body)
				loopID.insert(std::make_pair(block, i));
		}

		for (loop &lp : loops)
		{
			mapVar.clear();
			mapRegion.clear();
			failedVar.clear();
			vInvar.clear();

			insertPoint = nullptr;
			for (Block *pred : lp.header->preds)
				if (!lp.body.count(pred))
				{
					assert(!insertPoint);
					insertPoint = pred;
				}

			assert(insertPoint->brTrue.get() == lp.header);
			if (insertPoint->brFalse)
				break;
			assert(!insertPoint->brFalse);
			
			createInvars(lp);

			for (size_t i = 0; i < vInvar.size(); i++)
			{
				if (!vInvar[i]->failed)
					vInvar[i]->init();
			}

			for (size_t i = 0; i < vInvar.size(); i++)
			{
				InvariantRegion *region = dynamic_cast<InvariantRegion *>(vInvar[i].get());
				if (!region)
					break;
				if (region->failed)
					continue;

				InvariantRegion *upmostParent = region;
				while (mapRegion.count(upmostParent->node->parent.lock().get()) && !vInvar[mapRegion[upmostParent->node->parent.lock().get()]]->failed)
					upmostParent = dynamic_cast<InvariantRegion *>(vInvar[mapRegion[upmostParent->node->parent.lock().get()]].get());

				if (upmostParent != region)
				{
					region->worked = true;
					for (size_t dependency : region->dependOn)
					{
						if (dynamic_cast<InvariantRegion *>(vInvar[dependency].get()))
							continue;

						if (!upmostParent->dependBy.count(dependency))
						{
							upmostParent->dependOn.insert(dependency);
							vInvar[dependency]->dependBy.insert(upmostParent->index);
						}
						vInvar[dependency]->dependBy.erase(region->index);
					}
					for (size_t i : region->dependBy)
					{
						if (auto ptr = dynamic_cast<InvariantRegion *>(vInvar[i].get()))
						{
							ptr->dependOn.erase(region->index);
							continue;
						}
						upmostParent->dependBy.insert(i);
						vInvar[i]->dependOn.insert(upmostParent->index);
						vInvar[i]->dependOn.erase(region->index);
					}
				}
			}

			std::queue<size_t> workList;
			for (size_t i = 0; i < vInvar.size(); i++)
			{
				if (!vInvar[i]->failed && vInvar[i]->dependOn.empty())
					workList.push(i);
			}
			//std::cerr << "IN" << std::endl;
			while (!workList.empty())
			{
				size_t current = workList.front();
				workList.pop();
				if (vInvar[current]->failed)
					continue;

				vInvar[current]->work();
				for (size_t dependency : vInvar[current]->dependBy)
				{
					assert(!vInvar[dependency]->failed);
					vInvar[dependency]->dependOn.erase(current);
					if (!vInvar[dependency]->worked && vInvar[dependency]->dependOn.empty())
						workList.push(dependency);
				}
			}
			//std::cerr << "OUT" << std::endl;
		}
	}

	void LoopInvariantOptimizer::computeMaxVer()
	{
		func.inBlock->traverse([this](Block *block) -> bool
		{
			for (auto &ins : block->instructions())
			{
				for (Operand *operand : join<Operand *>(ins.getInputReg(), ins.getOutputReg()))
					maxVer[operand->val] = std::max(maxVer[operand->val], operand->ver);
			}
			return true;
		});
	}

	void LoopInvariantOptimizer::createInvars(const loop &lp)
	{
		std::shared_ptr<PSTNode> node(lp.header->pstNode.lock());
		std::function<void(PSTNode *)> traverse;
		traverse = [&traverse, this](PSTNode *node)
		{
			for (auto &child : node->children)
				traverse(child.get());
			InvariantRegion::construct(*this, node);
		};
		for (auto &child : node->children)
		{
			if (lp.body.count(child->inBlock))
				traverse(child.get());
		}

		failedVar.clear();
		for (Block *block : lp.body)
		{
			bool blockFailed = false;
			for (auto iter = block->ins.begin(); iter != block->ins.end(); ++iter)
			{
				auto &ins = *iter;
				auto output = ins.getOutputReg();
				if (ins.oper == Call && (ins.src1.type != Operand::funcID || !(program->vFuncs[ins.src1.val].attribute & ConstExpr))
					|| ins.oper == Load || ins.oper == LoadA
					|| ins.oper == Store || ins.oper == StoreA)
				{
					blockFailed = true;
					for (Operand *operand : output)
						failedVar.insert(*operand);
				}
				else if (!output.empty())
				{
					InvariantVar::construct(*this, block, iter);
				}
			}
			for (auto &kv : block->phi)
				InvariantVar::construct(*this, block, kv.second);
			if (blockFailed)
				failedBlock.insert(block);
		}
	}

	void LoopInvariantOptimizer::InvariantVar::init()
	{
		if (inited)
			return;
		if (isPhi)
			return fail();
		std::set<size_t> dependency;
		for (Operand *operand : insn->getInputReg())
		{
			if (parent.failedVar.count(*operand))
				return fail();
			if (parent.mapVar.count(*operand))
			{
				assert(dynamic_cast<InvariantVar *>(parent.vInvar[parent.mapVar[*operand]].get()));
				dependency.insert(parent.mapVar[*operand]);
			}
		}
		if (!setDependOn(dependency))
			return fail();
		inited = true;
	}

	void LoopInvariantOptimizer::InvariantVar::fail()
	{
		Invariant::fail();
		if (isPhi)
			parent.failedVar.insert(phiDst);
		else
		{
			for (Operand *operand : insn->getOutputReg())
				parent.failedVar.insert(*operand);
		}
	}

	void LoopInvariantOptimizer::InvariantRegion::init()
	{
		if (!parent.mapRegion.count(node->parent.lock().get()) && node->isSingleBlock())
			return fail();
		for (Block *block : node->blocks)
		{
			if (parent.failedBlock.count(block))
				return fail();
		}
		std::set<size_t> dependency;
		for (auto &child : node->children)
		{
			if (parent.vInvar[parent.mapRegion[child.get()]]->failed)
				return fail();
			dependency.insert(parent.mapRegion[child.get()]);
		}

		std::vector<size_t> children;
		std::function<void(PSTNode *)> dfs;
		dfs = [&children, &dfs, this](PSTNode *node)
		{
			assert(parent.vInvar[parent.mapRegion[node]]->inited);
			children.push_back(parent.mapRegion[node]);
			for(auto &child : node->children)
				dfs(child.get());
		};
		for (auto &child : node->children)
			dfs(child.get());

		auto isDefined = [&dependency, &children, this](Operand operand)
		{
			assert(!parent.failedVar.count(operand));
			if (!parent.mapVar.count(operand))
				return true;
			if (dependBy.count(parent.mapVar[operand]))
				return true;
			for (size_t child : children)
			{
				if (parent.vInvar[child]->dependBy.count(parent.mapVar[operand]))
					return true;
			}
			return false;
		};

		for (Block *block : node->blocks)
			for (auto &ins : block->instructions())
			{
				for (Operand *operand : ins.getOutputReg())
				{
					parent.vInvar[parent.mapVar[*operand]]->setDependOn({ index });
					parent.vInvar[parent.mapVar[*operand]]->inited = true;
				}
			}
		for(Block *block : node->blocks)
			for (auto &ins : block->instructions())
			{
				for (Operand *operand : ins.getInputReg())
				{
					if (parent.failedVar.count(*operand))
						return fail();
					if (!isDefined(*operand))
						dependency.insert(parent.mapVar[*operand]);
				}
			}
		if (!setDependOn(dependency))
			return fail();
		inited = true;
	}

	void LoopInvariantOptimizer::InvariantRegion::releaseVars()
	{
		std::vector<InvariantVar *> cand;
		for (size_t dependency : dependBy)
		{
			if (InvariantVar *var = dynamic_cast<InvariantVar *>(parent.vInvar[dependency].get()))
			{
				var->inited = false;
				cand.push_back(var);
			}
		}
		for (InvariantVar *var : cand)
		{
			var->init();
		}
	}

	void LoopInvariantOptimizer::InvariantRegion::fail()
	{
		releaseVars();
		for (auto &child : node->children)
		{
			if (child->isSingleBlock())
			{
				if (!parent.vInvar[parent.mapRegion[child.get()]]->failed)
					parent.vInvar[parent.mapRegion[child.get()]]->fail();
			}
		}
		Invariant::fail();
	}

	void LoopInvariantOptimizer::protectDivisor(Block *block, std::list<Instruction>::iterator insn)
	{
		if (insn->oper == Div || insn->oper == Mod)
		{
			if (insn->src2.isImm())
				return;
			assert(insn->src2.isReg());
			Operand tmp = RegSize(maxVer.empty() ? 0 : maxVer.rbegin()->first + 1, insn->src2.size());
			maxVer[tmp.val] = 0;
			block->ins.insert(insn, IR(tmp, TestZero, insn->src2, ImmSize(1, insn->src1.size())));
			insn->src2 = tmp;
		}
	}

	void LoopInvariantOptimizer::InvariantVar::work()
	{
		if (worked)
			return;
		assert(!isPhi);
		if (insn->oper == Move && insn->src1.isConst() && dependBy.empty())
			return;
		auto iter = parent.insertPoint->ins.insert(std::prev(parent.insertPoint->ins.end()), *insn);
		parent.protectDivisor(parent.insertPoint, iter);
		block->ins.erase(insn);
		worked = true;
	}

	void LoopInvariantOptimizer::InvariantRegion::work()
	{
		if (worked)
			return;
		std::shared_ptr<PSTNode> pstParent = node->parent.lock();

		std::set<Block *> innerBlocks;
		node->traverse([&innerBlocks](PSTNode *node)
		{
			for (Block *block : node->blocks)
				innerBlocks.insert(block);
		});

		Block *pred = nullptr;
		for (Block *block : node->inBlock->preds)
		{
			if (!innerBlocks.count(block))
			{
				assert(!pred);
				pred = block;
			}
		}

		Block::block_ptr &next =
			innerBlocks.count(node->outBlock->brTrue.get()) ? node->outBlock->brFalse : node->outBlock->brTrue;
		assert(next);

		std::shared_ptr<Block> tmpBlock(Block::construct());

		auto range = parent.loopID.equal_range(node->inBlock);
		for (auto iter = range.first; iter != range.second; ++iter)
		{
			if (parent.loops[iter->second].header != node->inBlock)
			{
				for (Block *block : innerBlocks)
					parent.loops[iter->second].body.erase(block);
				parent.loops[iter->second].body.insert(tmpBlock.get());
			}
		}

		std::shared_ptr<Block> inBlock = node->inBlock->self.lock();

		tmpBlock->pstNode = pstParent;
		pstParent->blocks.insert(tmpBlock.get());
		tmpBlock->brTrue = next->self.lock();
		tmpBlock->ins = { IRJump() };
		if (pred->brTrue.get() == inBlock.get())
			pred->brTrue = tmpBlock;
		if (pred->brFalse.get() == inBlock.get())
			pred->brFalse = tmpBlock;

		for (auto &kv : next->phi)
			for (auto &src : kv.second.srcs)
				if (src.second.lock().get() == node->outBlock)
					src.second = tmpBlock;

		for (auto &kv : parent.insertPoint->brTrue->phi)
			for (auto &src : kv.second.srcs)
				if (src.second.lock().get() == parent.insertPoint)
					src.second = node->outBlock->self;

		for (auto &kv : node->inBlock->phi)
			for (auto &src : kv.second.srcs)
				if (src.second.lock().get() == pred)
					src.second = parent.insertPoint->self;

		next = parent.insertPoint->brTrue;
		parent.insertPoint->brTrue = inBlock;
		parent.insertPoint = node->outBlock;

		std::function<void(PSTNode *)> dfs;
		dfs = [&dfs, this](PSTNode *node)
		{
			for (Block *block : node->blocks)
				for (auto iter = block->ins.begin(); iter != block->ins.end(); ++iter)
					parent.protectDivisor(block, iter);

			for (auto &child : node->children)
				dfs(child.get());
		};
		dfs(node);

		std::shared_ptr<PSTNode> ptr = node->self.lock();
		pstParent->children.erase(ptr->iterParent);
		parent.func.pstRoot->children.push_back(ptr);
		ptr->iterParent = std::prev(parent.func.pstRoot->children.end());

		for (size_t dependency : dependBy)
			parent.vInvar[dependency]->worked = true;

		worked = true;
	}
}