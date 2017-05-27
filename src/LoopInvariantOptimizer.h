#ifndef MX_COMPILER_LOOP_INVARIANT_OPTIMIZER_H
#define MX_COMPILER_LOOP_INVARIANT_OPTIMIZER_H

#include "common.h"
#include "IR.h"
#include "MxProgram.h"

namespace MxIR
{
	class LoopInvariantOptimizer
	{
	public:
		LoopInvariantOptimizer(Function &func) : func(func), program(MxProgram::getDefault()) {}
		void work();

	protected:
		struct loop
		{
			Block *header;
			std::set<Block *> body;
		};
		struct Invariant
		{
			LoopInvariantOptimizer &parent;
			std::set<size_t> dependBy;
			std::set<size_t> dependOn;
			bool failed, inited, worked;
			const size_t index;

			Invariant(LoopInvariantOptimizer &parent, size_t index) : parent(parent), failed(false), inited(false), worked(false), index(index) {}
			virtual ~Invariant() {}
			virtual void init() = 0;
			virtual void work() = 0;
			virtual void fail()
			{
				if (failed)
					return;
				failed = true;
				for (size_t invar : std::set<size_t>(dependBy))
					parent.vInvar[invar]->fail();
				for (size_t invar : dependOn)
					parent.vInvar[invar]->dependBy.erase(index);
			}
			bool setDependOn(const std::set<size_t> &newDepend)
			{
				for (size_t i : dependOn)
					parent.vInvar[i]->dependBy.erase(index);
				dependOn = newDepend;
				for (size_t i : dependOn)
				{
					if (parent.vInvar[i]->failed)
						return false;
				}
				for (size_t i : dependOn)
					parent.vInvar[i]->dependBy.insert(index);
				return true;
			}
		};
		struct InvariantVar : public Invariant
		{
			Block *block;
			std::list<Instruction>::iterator insn;
			Operand phiDst;
			bool isPhi;

			InvariantVar(LoopInvariantOptimizer &parent, size_t index, Block *block, const Block::PhiIns &phi) : Invariant(parent, index), block(block), isPhi(true), phiDst(phi.dst) {}
			InvariantVar(LoopInvariantOptimizer &parent, size_t index, Block *block, std::list<Instruction>::iterator insn) :
				Invariant(parent, index), block(block), insn(insn), isPhi(false) {}

			static InvariantVar *construct(LoopInvariantOptimizer &parent, Block *block, const Block::PhiIns &phi)
			{
				parent.vInvar.emplace_back(new InvariantVar(parent, parent.vInvar.size(), block, phi));
				InvariantVar *ptr = dynamic_cast<InvariantVar *>(parent.vInvar.back().get());
				parent.mapVar[ptr->phiDst] = ptr->index;
				return ptr;
			}
			static InvariantVar *construct(LoopInvariantOptimizer &parent, Block *block, std::list<Instruction>::iterator insn)
			{
				parent.vInvar.emplace_back(new InvariantVar(parent, parent.vInvar.size(), block, insn)); 
				InvariantVar *ptr = dynamic_cast<InvariantVar *>(parent.vInvar.back().get());
				parent.mapVar[ptr->insn->dst] = ptr->index;
				return ptr;
			}

			virtual void init() override;
			virtual void fail() override;
			virtual void work() override;
		};
		struct InvariantRegion : public Invariant
		{
			PSTNode *node;

			InvariantRegion(LoopInvariantOptimizer &parent, size_t index, PSTNode *node) : Invariant(parent, index), node(node) {}

			static InvariantRegion *construct(LoopInvariantOptimizer &parent, PSTNode *node)
			{
				parent.vInvar.emplace_back(new InvariantRegion(parent, parent.vInvar.size(), node));
				InvariantRegion *ptr = dynamic_cast<InvariantRegion *>(parent.vInvar.back().get());
				parent.mapRegion[ptr->node] = ptr->index;
				return ptr;
			}

			virtual void init() override;
			virtual void fail() override;
			virtual void work() override;

			void releaseVars();
		};

	protected:
		void computeMaxVer();
		void createInvars(const loop &lp);
		void protectDivisor(Block *block, std::list<Instruction>::iterator insn);

	protected:
		Function &func;
		MxProgram *program;
		std::vector<std::unique_ptr<Invariant>> vInvar;
		std::map<Operand, size_t> mapVar;
		std::map<PSTNode *, size_t> mapRegion;

		Block *insertPoint;

		std::set<Operand> failedVar;
		std::set<Block *> failedBlock;

		std::map<size_t, size_t> maxVer;

		std::vector<loop> loops;
		std::multimap<Block *, size_t> loopID;
	};
}

#endif