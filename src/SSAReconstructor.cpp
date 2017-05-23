#include "common_headers.h"
#include "SSAReconstructor.h"

namespace MxIR
{
	void SSAReconstructor::preprocess()
	{
		func.inBlock->traverse([this](Block *block) -> bool
		{
			property[block].idx = vBlock.size();
			vBlock.push_back(block);
			return true;
		});
		dtree = DomTree(vBlock.size());
		for (Block *block : vBlock)
			for (Block *next : { block->brTrue.get(), block->brFalse.get() })
			{
				if(next)
					dtree.link(property[block].idx, property[next].idx);
			}
		assert(property[func.inBlock.get()].idx == 0);
		dtree.buildTree(0);
	}

	void SSAReconstructor::calcIDF(Block *block)
	{
		std::queue<Block *> Q;
		for (size_t frontier : dtree.getDomFrontier(property[block].idx))
		{
			Q.push(vBlock[frontier]);
			property[block].idf.insert(vBlock[frontier]);
		}

		while (!Q.empty())
		{
			Block *cur = Q.front();
			Q.pop();
			for (size_t frontier : dtree.getDomFrontier(property[cur].idx))
				if (!property[block].idf.count(vBlock[frontier]))
				{
					Q.push(vBlock[frontier]);
					property[block].idf.insert(vBlock[frontier]);
				}
		}

		property[block].visited = true;
	}

	void SSAReconstructor::reconstruct(const std::vector<size_t> &vars)
	{
		curVer.clear();
		defIDF.clear();
		func.inBlock->traverse([&vars, this](Block *block) -> bool
		{
			std::set<size_t> defs;
			property[block].latestVer.clear();
			for (auto &ins : block->instructions())
			{
				for (Operand *operand : ins.getOutputReg())
				{
					for (size_t var : vars)
						if (operand->val == var)
						{
							if(!varOp.count(var))
								varOp[var] = operand->clone().setPRegID(-1);
							else
							{
								assert(varOp[var].type == operand->type);
							}
							property[block].latestVer[var] = operand->ver = ++curVer[var];
							defs.insert(var);
						}
				}
			}
			if (!defs.empty() && !property[block].visited)
				calcIDF(block);
			for (size_t def : defs)
			{
				for (Block *idf : property[block].idf)
					defIDF[def].insert(idf);
			}
			return true;
		});

		func.inBlock->traverse([&vars, this](Block *block) -> bool
		{
			std::map<size_t, size_t> latestVer;
			for (auto &kv : block->phi)
			{
				if (kv.second.dst.ver != 0)
					latestVer[kv.second.dst.val] = kv.second.dst.ver;
				for (auto &src : kv.second.srcs)
				{
					for(size_t var : vars)
						if (src.first.val == var)
						{
							size_t version = findDefFromBottom(var, src.second.lock().get());
							src.first.ver = version;
						}
				}
			}
			
			for (auto &ins : block->ins)
			{
				for (Operand *operand : ins.getInputReg())
				{
					for(size_t var : vars)
						if (operand->val == var)
						{
							if (latestVer.count(var))
								operand->ver = latestVer[var];
							else
								operand->ver = findDefFromTop(var, block);
						}
				}
				for (Operand *operand : ins.getOutputReg())
				{
					if (operand->ver != 0)
						latestVer[operand->val] = operand->ver;
				}
			}
			return true;
		});
	}

	size_t SSAReconstructor::findDefFromBottom(size_t var, Block *block)
	{
		if (property[block].latestVer.count(var))
			return property[block].latestVer[var];
		else
			return findDefFromTop(var, block);
	}

	size_t SSAReconstructor::findDefFromTop(size_t var, Block *block)
	{
		if (defIDF[var].count(block))
		{
			if (block->phi.count(var))
				return block->phi[var].dst.ver;
			Block::PhiIns phi;
			phi.dst = varOp[var];
			phi.dst.ver = ++curVer[var];
			if(!property[block].latestVer.count(var))
				property[block].latestVer[var] = phi.dst.ver;
			for (Block *pred : block->preds)
			{
				Operand src = varOp[var];
				src.ver = findDefFromBottom(var, pred);
				phi.srcs.push_back(std::make_pair(src, pred->self));
			}
			block->phi.insert(std::make_pair(var, phi));
			return phi.dst.ver;
		}
		else
			return findDefFromBottom(var, vBlock[dtree.getIdom(property[block].idx)]);
	}

	void SSAReconstructor::reconstructAuto()
	{
		std::vector<size_t> vars;
		std::map<size_t, size_t> freq;
		func.inBlock->traverse([&freq, &vars](Block *block) -> bool
		{
			for (auto &ins : block->instructions())
			{
				for (Operand *operand : ins.getOutputReg())
				{
					if (operand->val == Operand::InvalidID)
						continue;
					freq[operand->val]++;
					if (freq[operand->val] == 2)
						vars.push_back(operand->val);
				}
			}
			return true;
		});

		reconstruct(vars);
	}
}