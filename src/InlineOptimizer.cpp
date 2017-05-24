#include "common_headers.h"
#include "InlineOptimizer.h"
#include "utils/JoinIterator.h"

namespace MxIR
{
	size_t InlineOptimizer::statFunc::penalty() const
	{
		if (forceInline)
			return 0;
		bool hasFuncCall = externalCall ? true : !callTo.empty();
		return nInsn + nBlock * nBlock + (hasFuncCall ? 100 : 0);
	}

	void InlineOptimizer::work()
	{
		const size_t maxPenalty = CompileFlags::getInstance()->inline_param;
		const int threshold = CompileFlags::getInstance()->inline_param2;
		struct Tqueue
		{
			size_t idx, nUpdate, penalty;
			bool operator<(const Tqueue &rhs) const { return penalty > rhs.penalty; }
		};
		std::priority_queue<Tqueue> Q;
		analyzeProgram();
		for (size_t i = 0; i < stats.size(); i++)
		{
			if (program->vFuncs[i].attribute & NoInline)
				continue;
			Q.push(Tqueue{ i, stats[i].nUpdate, stats[i].penalty() });
		}
		while (!Q.empty())
		{
			Tqueue cur = Q.top();
			Q.pop();
			if (cur.nUpdate != stats[cur.idx].nUpdate)
				continue;
			if (cur.penalty > maxPenalty)
				break;
			Function content = program->vFuncs[cur.idx].content.clone();
			if (stats[cur.idx].callTo.count(cur.idx))
			{
				size_t incBlock = (stats[cur.idx].nBlock - 1) * stats[cur.idx].callTo[cur.idx];
				if (incBlock > threshold + stats[cur.idx].nBlock * sqrt(threshold))
					continue;

				applyInline(cur.idx, cur.idx, content);
				stats[cur.idx].nUpdate++;
				Q.push(Tqueue{ cur.idx, stats[cur.idx].nUpdate, stats[cur.idx].penalty() });
			}
			else
			{
				bool flag = true;
				for (size_t i = 0; i < stats.size(); i++)
					if (stats[i].callTo.count(cur.idx))
					{
						size_t incBlock = (stats[cur.idx].nBlock - 1) * stats[i].callTo[cur.idx];
						if (incBlock > threshold + stats[i].nBlock * sqrt(threshold))
						{
							flag = false;
							continue;
						}

						applyInline(cur.idx, i, content);
						assert(!stats[i].callTo.count(cur.idx));
						stats[i].nUpdate++;
						Q.push(Tqueue{ i, stats[i].nUpdate, stats[i].penalty() });
					}
				if(flag && !(program->vFuncs[cur.idx].attribute & Export))
					program->vFuncs[cur.idx].disabled = true;
			}
		}
	}

	void InlineOptimizer::analyzeProgram()
	{
		stats.resize(program->vFuncs.size());
		for (size_t i = 0; i < program->vFuncs.size(); i++)
		{
			stats[i] = analyzeFunc(program->vFuncs[i].content);
			if (program->vFuncs[i].attribute & ForceInline)
				stats[i].forceInline = true;
			else
				stats[i].forceInline = false;
		}
	}

	InlineOptimizer::statFunc InlineOptimizer::analyzeFunc(Function &func)
	{
		statFunc stat;
		stat.nVar = 0;
		for (auto &param : func.params)
		{
			assert(param.isReg());
			stat.nVar = std::max(stat.nVar, param.val + 1);
		}
		func.inBlock->traverse([&stat](Block *block) -> bool
		{
			stat.nBlock++;
			stat.nInsn += block->ins.size();
			for (auto &ins : block->ins)
			{
				if (ins.oper == Call)
				{
					if (ins.src1.type == Operand::funcID)
						stat.callTo[ins.src1.val]++;
					else if (ins.src1.type == Operand::externalSymbolName)
						stat.externalCall = true;
				}
				for (Operand *operand : join<Operand *>(ins.getInputReg(), ins.getOutputReg()))
					stat.nVar = std::max(stat.nVar, operand->val + 1);
			}
			return true;
		});
		return stat;
	}

	void InlineOptimizer::applyInline(size_t callee, size_t caller, Function &content)
	{
		std::vector<Block *> vBlocks;
		program->vFuncs[caller].content.inBlock->traverse([&vBlocks](Block *block) -> bool
		{
			vBlocks.push_back(block);
			return true;
		});
		auto callTo = stats[callee].callTo;

		for(Block *block : vBlocks)
		{
			//auto endIter = block->ins.end();
			for (auto iter = block->ins.begin(); iter != block->ins.end(); )
			{
				if (iter->oper == Call && iter->src1.type == Operand::funcID && iter->src1.val == callee)
				{
					Operand retVar = iter->dst;
					Function child = content.clone();
					assert(child.outBlock->ins.empty());
					size_t offsetVarID = stats[caller].nVar;

					stats[caller].nVar += stats[callee].nVar;
					stats[caller].nInsn += stats[callee].nInsn + child.params.size();
					if (stats[callee].nBlock > 2)
						stats[caller].nBlock += stats[callee].nBlock - 1;
					for (auto &kv : callTo)
						stats[caller].callTo[kv.first] += kv.second;
					stats[caller].callTo[callee]--;
					

					child.inBlock->traverse([offsetVarID, &retVar, &child](Block *block) -> bool
					{
						for (auto iter = block->ins.begin(); iter != block->ins.end(); ++iter)
						{
							for (Operand *operand : join<Operand *>(iter->getInputReg(), iter->getOutputReg()))
								operand->val += offsetVarID;
							if (iter->oper == Return)
							{
								assert(std::next(iter) == block->ins.end() && block->brTrue.get() == child.outBlock.get());
								if (retVar.type == Operand::empty)
									*iter = IRJump();
								else if (iter->src1.type == Operand::empty)
								{
									block->ins.insert(iter, IR(retVar, Move, ImmSize(0, retVar.size())));
									*iter = IRJump();
								}
								else
								{
									block->ins.insert(iter, IR(retVar, Move, iter->src1));
									*iter = IRJump();
								}
							}
						}
						return true;
					});
					assert(child.params.size() == iter->paramExt.size());
					for (auto iterJ = child.inBlock->ins.cbegin(); iterJ != child.inBlock->ins.cend();)
					{
						if (iterJ->oper == Allocate)
						{
							program->vFuncs[caller].content.inBlock->ins.push_front(*iterJ);
							iterJ = child.inBlock->ins.erase(iterJ);
						}
						else
							++iterJ;
					}
					for (auto &param : child.params)
						param.val += offsetVarID;
					for (size_t i = 0; i < child.params.size(); i++)
						child.inBlock->ins.push_front(IR(child.params[i], Move, iter->paramExt[i]));

					if (child.inBlock->brTrue.get() == child.outBlock.get() && !child.inBlock->brFalse)
					{
						assert(child.inBlock->ins.back().oper == Jump);
						child.inBlock->ins.pop_back();
						block->ins.splice(iter, child.inBlock->ins);
						iter = block->ins.erase(iter);
					}
					else
					{
						iter = block->ins.erase(iter);
						child.outBlock->ins.splice(child.outBlock->ins.end(), block->ins, iter, block->ins.end());
						block->ins.splice(block->ins.end(), child.inBlock->ins);
						child.outBlock->brTrue = block->brTrue;
						child.outBlock->brFalse = block->brFalse;
						block->brTrue = child.inBlock->brTrue;
						block->brFalse = child.inBlock->brFalse;
						
						block = child.outBlock.get();
						iter = block->ins.begin();
					}
				}
				else
					++iter;
			}
		}

		//assert(caller == callee || stats[caller].callTo[callee] == 0);
		if(stats[caller].callTo[callee] == 0)
			stats[caller].callTo.erase(callee);
	}

}