#ifndef MX_COMPILER_REGISTER_ALLOCATOR_SSA_H
#define MX_COMPILER_REGISTER_ALLOCATOR_SSA_H

#include "common.h"
#include "IR.h"
#include "utils/DomTree.h"

namespace MxIR
{
	class RegisterAllocatorSSA
	{
	public:
		explicit RegisterAllocatorSSA(Function &func, const std::vector<int> &phyReg) : func(func), phyReg(phyReg) {}
		void work();

	protected:
		void splitCriticalEdge();
		void computeDomTree();
		void computeLoop();
		void relabelVReg();
		void computeDefUses();
		void computeNextUse();
		void computeMaxPressure();
		void computeVarOp();
		void computeVarGroup();
		void computeExternalVar();

		void spillRegister();
		void spillRegisterBlock(Block *block);
		Instruction getLoadInsn(size_t regid);
		Instruction getSpillInsn(size_t regid);
		void initLoopHeader(Block *block);		//init the W set (set of the v-registers not spilled) of loop header
		void initUsualBlock(Block *block);

		bool isSpill(const Instruction &insn);
		bool isReload(const Instruction &insn);
		void eliminateSpillCode();
		void eliminateSpillCode(size_t idx, std::set<size_t> &spilled);
		void insertAllocateCode();

		void reconstructSSA();
		// ---    vars will be relabeled here     ---
		// --- varOp and varGroup is invalid here ---
		void analysisLiveness();
		void buildInterferenceGraph();
		void allocateRegister();
		void allocateRegisterDFS(size_t idx);
		int chooseRegister(size_t vreg, const std::vector<int> &prefer);

		int getPReg(Operand operand);
		void coalesce();

		void destructSSA();
		void writeRegInfo();

		size_t findVertexRoot(size_t vtx);
		void mergeVertices(const std::set<size_t> &vtxList);

	protected:
		struct BlockProperty
		{
			size_t idx;
			std::map<size_t, size_t> nextUseBegin, nextUseEnd;	//global next use distance at the begin/end of the block
			std::set<Block *> loopBorder;						//edges leading out of loop
			std::set<Block *> loopBody;

			std::set<size_t> definedVar;		//virtual register that is defined in this block
			std::set<size_t> usedVar;
			// std::set<size_t> updateDistance;	//virtual register of which the next use distance at end is updated. used in computeNextUse

			size_t maxPressure;

			std::set<size_t> Wentry, Wexit;

			bool visited;		//visited in spill stage

			std::set<size_t> liveIn, liveOut;	//used when building inference graph

			/*
				------- liveIn ---------
				a = phi(...)
				b = phi(...)
				...
				----- nextUseBegin -----
				c = d op e
				...
				- nextUseEnd & liveOut -
			*/

			BlockProperty() : idx(size_t(-1)), maxPressure(0), visited(false) {}
		};
		struct GraphVertex
		{
			std::set<size_t> neighbor;
			std::shared_ptr<std::set<int>> forbiddenReg;
			int preg;
			bool pinned;
			size_t root;

			GraphVertex() : preg(-1), pinned(false) {}
		};
		struct OptimUnit
		{
			RegisterAllocatorSSA &allocator;
			size_t keyVertex;
			int targetRegister;
			std::set<size_t> S;
			std::map<size_t, int> changedReg;

			std::set<size_t> cand;
			const size_t minCand;	//if cand < minCand, this OptimUnit will be discarded
			const ssize_t bias;

			OptimUnit(RegisterAllocatorSSA &allocator, size_t minCand, ssize_t bias) : allocator(allocator), minCand(minCand), bias(bias) {}

			virtual void fail(size_t u) = 0;				//u != keyVertex
			virtual void conflict(size_t u, size_t v) = 0;	//u, v != keyVertex
			virtual void apply();

			bool operator<(const OptimUnit &rhs) const { return S.size() + bias < rhs.S.size() + rhs.bias; }
			int work();	//>0 for success; =0 for retry; <0 for abandoned
			ssize_t adjust(size_t src);	//return -1 if succeed. return the id of conflicted var otherwise
			ssize_t adjust(size_t src, int target);
			int getCurrentReg(size_t idx) { return changedReg.count(idx) ? changedReg[idx] : allocator.ifGraph[idx].preg; }
		};
		struct OptimUnitAll : public OptimUnit
		{
			std::set<size_t> vertices;	//including keyVertex
			std::set<std::pair<size_t, size_t>> edges;

			OptimUnitAll(RegisterAllocatorSSA &allocator, size_t key, const std::vector<size_t> &another);

			virtual void fail(size_t u) override;
			virtual void conflict(size_t u, size_t v) override;

			void findMaxStableSet();
		};
		struct OptimUnitOne : public OptimUnit
		{
			size_t another;

			OptimUnitOne(RegisterAllocatorSSA &allocator, size_t u, size_t v);

			virtual void fail(size_t u) override { S.clear(); }
			virtual void conflict(size_t u, size_t v) override { S.clear(); }
		};
		struct OptimUnitSingle : public OptimUnit	//set key var to a specific register
		{
			OptimUnitSingle(RegisterAllocatorSSA &allocator, size_t key, int target) : OptimUnit(allocator, 1, 0)
			{
				targetRegister = target;
				keyVertex = allocator.findVertexRoot(key);
				S.insert(keyVertex);
			}
			virtual void apply() override
			{
				assert(S.size() == 1);
				allocator.ifGraph[*S.begin()].pinned = true;
			}
			virtual void fail(size_t u) override { S.clear(); }
			virtual void conflict(size_t u, size_t v) override { S.clear(); }
		};
		std::vector<GraphVertex> ifGraph;
		std::map<Block *, BlockProperty> property;
		std::vector<Block *> vBlock;
		DomTree dtree;

		std::vector<Operand> varOp;
		std::vector<size_t> varGroup;	//vregid -> groupid. note that groupid is also the store address of the register
		std::map<size_t, Operand> externalVarHint;	//varid -> alloc hint
		size_t nVar;

		Function &func;
		std::vector<int> phyReg;

		static const size_t outLoopPenalty = 1000;
	};
}

#endif