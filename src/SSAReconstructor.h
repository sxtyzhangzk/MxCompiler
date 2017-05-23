#ifndef MX_COMPILER_SSA_RECONSTRUCTOR_H
#define MX_COMPILER_SSA_RECONSTRUCTOR_H

#include "common.h"
#include "IR.h"
#include "utils/DomTree.h"

namespace MxIR
{
	class SSAReconstructor
	{
	public:
		explicit SSAReconstructor(Function &func) : func(func) {}
		void preprocess();
		void reconstruct(const std::vector<size_t> &vars);	//assume all vreg in IR has no version information
		void reconstructAuto();	//reconstruct ssa by all vars that have duplicated definitions

	protected:
		void calcIDF(Block *block);
		size_t findDefFromBottom(size_t var, Block *block);	//return the latest version
		size_t findDefFromTop(size_t var, Block *block);

	protected:
		struct BlockProperty
		{
			size_t idx;
			std::set<Block *> idf;	//iterated dominance frontier
			bool visited;			//visited when calculating idf
			std::map<size_t, size_t> latestVer;

			BlockProperty() : idx(size_t(-1)), visited(false) {}
		};
		std::map<Block *, BlockProperty> property;
		std::vector<Block *> vBlock;

		DomTree dtree;
		Function &func;

		std::map<size_t, Operand> varOp;
		std::map<size_t, size_t> curVer;
		std::map<size_t, std::set<Block *>> defIDF;	//the union of idf of each block that defines the target var  
	};
}

#endif