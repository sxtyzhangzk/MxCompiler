#include "common_headers.h"
#include "LoadCombine.h"

namespace MxIR
{
	void LoadCombine::combine(Block *block)
	{
		std::map<Operand, Operand> loadOp;	//addr -> var
		std::map<std::pair<Operand, Operand>, Operand> loadAOp;

		for (auto &ins : block->ins)
		{
			if (ins.oper == Call || ins.oper == Store || ins.oper == StoreA)
			{
				loadOp.clear();
				loadAOp.clear();
				continue;
			}
			if (ins.oper == Load)
			{
				if (loadOp.count(ins.src1) && loadOp[ins.src1].size() >= ins.dst.size())
					ins = IR(ins.dst, Move, loadOp[ins.src1].clone().setSize(ins.dst.size()));
				else
					loadOp[ins.src1] = ins.dst;
			}
			else if (ins.oper == LoadA)
			{
				auto src = std::make_pair(ins.src1, ins.src2);
				if (loadAOp.count(src) && loadAOp[src].size() >= ins.dst.size())
					ins = IR(ins.dst, Move, loadAOp[src].clone().setSize(ins.dst.size()));
				else
					loadAOp[src] = ins.dst;
			}
		}
	}

	void LoadCombine::work()
	{
		func.inBlock->traverse([](Block *block) -> bool
		{
			combine(block);
			return true;
		});
	}
}