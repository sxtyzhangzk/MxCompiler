#include "common_headers.h"
#include "InstructionSelect.h"

namespace MxIR
{
	void InstructionSelect::computeUseCount()
	{
		func.inBlock->traverse([this](Block *block) -> bool
		{
			for (auto &ins : block->instructions())
				for (Operand *operand : ins.getInputReg())
					useCount[*operand]++;
			return true;
		});
	}

	void InstructionSelect::selectInsn(Block *block)
	{
		static const std::set<Operation> alterInsn = {
			Slt, Sle, Seq, Sgt, Sge, Sne,
			Sltu, Sleu, Sgtu, Sgeu
		};
		if (block->ins.back().oper == Br && block->ins.size() >= 2)
		{
			auto iter = std::prev(block->ins.end(), 2);
			if (alterInsn.count(iter->oper))
			{
				if (iter->dst.val == block->ins.back().src1.val && iter->dst.ver == block->ins.back().src1.ver
					&& useCount[iter->dst] == 1)
				{
					iter->dst = EmptyOperand();
					block->ins.back().src1 = EmptyOperand();
				}
			}
		}
	}

	void InstructionSelect::work()
	{
		computeUseCount();
		func.inBlock->traverse([this](Block *block) -> bool
		{
			if(block != func.outBlock.get())
				selectInsn(block);
			return true;
		});
	}
}