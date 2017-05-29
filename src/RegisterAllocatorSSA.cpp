#include "common_headers.h"
#include "RegisterAllocatorSSA.h"
#include "LoopDetector.h"
#include "SSAReconstructor.h"
#include "utils/JoinIterator.h"
#include "utils/UnionFindSet.h"
#include "utils/MaxClique.h"

namespace MxIR
{
	struct needreg_filter_t
	{
		bool operator()(Operand operand) const { return operand.isReg() && operand.val != Operand::InvalidID && !operand.noreg; }
	};
	struct hasreg_filter_t 
	{
		bool operator()(Operand operand) const 
		{
			if (!operand.isReg())
				return false;
			if (operand.noreg)
				return false;
			if (operand.pregid != -1)
				return true;
			return operand.val != Operand::InvalidID;
		}
	};
	const needreg_filter_t needreg;
	const hasreg_filter_t hasreg;
	std::vector<Operand *> operator|(const std::vector<Operand *> &container, needreg_filter_t)
	{
		std::vector<Operand *> ret;
		std::copy_if(container.begin(), container.end(), std::back_inserter(ret), [](Operand *operand)
		{
			return needreg(*operand);
		});
		return ret;
	}
	std::vector<Operand *> operator|(const std::vector<Operand *> &container, hasreg_filter_t)
	{
		std::vector<Operand *> ret;
		std::copy_if(container.begin(), container.end(), std::back_inserter(ret), [](Operand *operand)
		{
			return hasreg(*operand);
		});
		return ret;
	}

	void RegisterAllocatorSSA::work()
	{
		splitCriticalEdge();

		computeDomTree();
		computeLoop();
		relabelVReg();
		computeVarOp();
		computeDefUses();
		computeVarGroup();
		computeNextUse();
		computeMaxPressure();
		computeExternalVar();
		
		spillRegister();
		eliminateSpillCode();
		insertAllocateCode();
		
		reconstructSSA();
		relabelVReg();
		computeDefUses();
		analysisLiveness();
		buildInterferenceGraph();
		allocateRegister();

		coalesce();

		/*writeRegInfo(); return;*/

		destructSSA();
		writeRegInfo();
	}

	void RegisterAllocatorSSA::splitCriticalEdge()
	{
		std::vector<Block *> blockList;
		func.inBlock->traverse([&blockList](Block *block) -> bool { blockList.push_back(block); return true; });

		std::map<Block *, std::map<Block *, std::weak_ptr<Block>>> newPred;	//block -> {old pred -> new pred}
		for (Block *block : blockList)
		{
			if (!block->brTrue || !block->brFalse)
				continue;
			if (block->brTrue.get() == block->brFalse.get())
			{
				block->ins.back() = IRJump();
				block->brFalse.reset();
				continue;
			}

			for (Block::block_ptr &next : { std::ref(block->brTrue), std::ref(block->brFalse) })
			{
				if (next->preds.size() <= 1)
					continue;
				std::shared_ptr<Block> midBlock = Block::construct();
				newPred[next.get()][block] = midBlock;
				midBlock->ins = { IRJump() };
				midBlock->brTrue = next;
				next = std::move(midBlock);
			}
		}

		for (Block *block : blockList)
		{
			if (!newPred.count(block))
				continue;
			for (auto &kv : block->phi)
			{
				for (auto &src : kv.second.srcs)
				{
					std::shared_ptr<Block> oldPred = src.second.lock();
					if (!newPred[block].count(oldPred.get()))
						continue;
					src.second = newPred[block][oldPred.get()];
				}
			}
		}
	}

	void RegisterAllocatorSSA::computeDomTree()
	{
		func.inBlock->traverse([this](Block *block) -> bool
		{
			property[block].idx = vBlock.size();
			vBlock.push_back(block);
			return true;
		});
		dtree = DomTree(vBlock.size());
		for (size_t i = 0; i < vBlock.size(); i++)
		{
			Block *blk = vBlock[i];
			for (Block *next : { blk->brTrue.get(), blk->brFalse.get() })
				if (next)
					dtree.link(i, property[next].idx);
		}
		assert(property[func.inBlock.get()].idx == 0);
		dtree.buildTree(0);
	}

	void RegisterAllocatorSSA::computeLoop()
	{
		LoopDetector loopDetector(func);
		loopDetector.findLoops();
		for (auto &kv : loopDetector.getLoops())
		{
			Block *header = kv.first;
			const std::set<Block *> &body = kv.second;
			for (Block *blk : body)
			{
				for (Block *next : { blk->brTrue.get(), blk->brFalse.get() })
					if (next && !body.count(next))
						property[blk].loopBorder.insert(next);
			}
			property[header].loopBody = body;
		}
	}

	void RegisterAllocatorSSA::relabelVReg()
	{
		std::map<Operand, size_t> newIndex;
		func.inBlock->traverse([&newIndex, this](Block *block) -> bool
		{
			for (auto &ins : block->instructions())
			{
				for (Operand *operand : join<Operand *>(ins.getInputReg(), ins.getOutputReg()))
				{
					if (operand->val == Operand::InvalidID)
						continue;
					auto iter = newIndex.find(*operand);
					if (iter != newIndex.end())
						operand->val = iter->second;
					else
					{
						size_t idx = newIndex.size();
						newIndex[*operand] = idx;
						operand->val = idx;
					}
					operand->ver = 0;
				}
			}
			std::map<size_t, Block::PhiIns> mapPhi;
			for (auto &kv : block->phi)
				mapPhi.insert(std::make_pair(kv.second.dst.val, kv.second));
			block->phi = std::move(mapPhi);
			return true;
		});
		nVar = newIndex.size();

		/*for (auto &kv : newIndex)
		{
			std::cerr << kv.first.val << "_" << kv.first.ver << "\t->\t" << kv.second << std::endl;
		}
		std::cerr << std::endl;*/
	}

	void RegisterAllocatorSSA::computeDefUses()
	{
		func.inBlock->traverse([this](Block *block) -> bool
		{
			BlockProperty &bp = property[block];
			bp.definedVar.clear();
			bp.usedVar.clear();
			for (auto &ins : block->ins)
			{
				for (Operand *operand : ins.getOutputReg())
				{
					if (operand->val == Operand::InvalidID)
						continue;
					bp.definedVar.insert(operand->val);
				}
				for (Operand *operand : ins.getInputReg())
				{
					if (operand->val == Operand::InvalidID)
						continue;
					bp.usedVar.insert(operand->val);
				}
			}
			return true;
		});
	}

	void RegisterAllocatorSSA::computeNextUse()
	{
		std::queue<Block *> Q;
		std::map<Block *, std::set<size_t>> updatedVar;
		auto updateNextUseOut = [&Q, &updatedVar, this](Block *block, size_t var, size_t newDistance)
		{
			BlockProperty &bp = property[block];
			if (!bp.nextUseEnd.count(var) || newDistance < bp.nextUseEnd[var])
			{
				bp.nextUseEnd[var] = newDistance;
				if (updatedVar[block].empty())
					Q.push(block);
				updatedVar[block].insert(var);
			}
		};
		auto updatePred = [&updateNextUseOut, this](Block *block, size_t var)
		{
			BlockProperty &bp = property[block];
			if (block->phi.count(var))
			{
				for (auto &src : block->phi[var].srcs)
				{
					if (!src.first.isReg())
						continue;
					std::shared_ptr<Block> pred = src.second.lock();
					BlockProperty &bpPred = property[pred.get()];
					size_t newDistance = bpPred.loopBorder.count(block) ? outLoopPenalty + bp.nextUseBegin[var] : bp.nextUseBegin[var];
					updateNextUseOut(pred.get(), src.first.val, newDistance);
				}
			}
			else
			{
				for (Block *pred : block->preds)
				{
					BlockProperty &bpPred = property[pred];
					size_t newDistance = bpPred.loopBorder.count(block) ? outLoopPenalty + bp.nextUseBegin[var] : bp.nextUseBegin[var];
					updateNextUseOut(pred, var, newDistance);
				}
			}
		};
		func.inBlock->traverse([&updatePred, this](Block *block) -> bool
		{
			BlockProperty &bp = property[block];
			ssize_t curInsn = block->ins.size() - 1;
			for (auto iter = block->ins.rbegin(); iter != block->ins.rend(); ++iter, curInsn--)
			{
				for (Operand *operand : iter->getOutputReg() | needreg)
					bp.nextUseBegin.erase(operand->val);
				for (Operand *operand : iter->getInputReg() | needreg)
					bp.nextUseBegin[operand->val] = curInsn;
			}
			for (auto &kv : bp.nextUseBegin)
				updatePred(block, kv.first);
			return true;
		});
		while (!Q.empty())
		{
			Block *block = Q.front();
			Q.pop();
			BlockProperty &bp = property[block];
			std::vector<size_t> vars;
			for (size_t var : updatedVar[block])
				vars.push_back(var);
			updatedVar[block].clear();
			for (size_t var : vars)
			{
				if (!bp.definedVar.count(var))
					if (!bp.nextUseBegin.count(var) || bp.nextUseEnd[var] + block->ins.size() < bp.nextUseBegin[var])
					{
						bp.nextUseBegin[var] = bp.nextUseEnd[var] + block->ins.size();
						updatePred(block, var);
					}
			}
		}

		func.inBlock->traverse([this](Block *block) -> bool
		{
			BlockProperty &bp = property[block];
			for (auto iter = block->phi.cbegin(); iter != block->phi.cend();)
			{
				if (!bp.nextUseBegin.count(iter->first))
					iter = block->phi.erase(iter);
				else
					++iter;
			}

			/*std::stringstream ss;
			ss << "nextUseBegin: ";
			for (auto &kv : bp.nextUseBegin)
			{
				ss << kv.first << ":" << kv.second << " ";
			}
			ss << std::endl; 
			ss << "nextUseEnd: ";
			for (auto &kv : bp.nextUseEnd)
			{
				ss << kv.first << ":" << kv.second << " ";
			}
			ss << std::endl;
			block->dbgInfo = ss.str();*/
			return true;
		});
	}

	void RegisterAllocatorSSA::computeMaxPressure()
	{
		for (auto &kvProperty : property)
		{
			Block *block = kvProperty.first;
			BlockProperty &bp = kvProperty.second;

			std::map<size_t, ssize_t> lastUse;		//vreg id -> last use instruction
			for (auto &kv : bp.nextUseEnd)
				lastUse[kv.first] = -1;

			ssize_t curIns = block->ins.size() - 1;
			for (auto iter = block->ins.rbegin(); iter != block->ins.rend(); iter++)
			{
				for (Operand *operand : iter->getInputReg() | needreg)
				{
					if (!lastUse.count(operand->val))
						lastUse[operand->val] = curIns;
				}
				curIns--;
			}

			std::map<ssize_t, size_t> lastUseInsn;	//instruction id -> count of last use vreg
			for (auto &kv : lastUse)
				lastUseInsn[kv.second]++;

			size_t pressure = bp.nextUseBegin.size();
			bp.maxPressure = pressure;
			curIns = 0;

			size_t lastLock = 0;
			for (auto &ins : block->ins)
			{
				if (ins.oper == LockReg)
				{
					assert(lastLock == 0);
					lastLock = ins.paramExt.size();
					pressure += lastLock;
					bp.maxPressure = std::max(bp.maxPressure, pressure);
				}
				else if (ins.oper == UnlockReg)
				{
					pressure -= lastLock;
					lastLock = 0;
				}

				if (lastUseInsn.count(curIns))
				{
					assert(lastUseInsn[curIns] <= pressure);
					pressure -= lastUseInsn[curIns];
				}
				for (Operand *operand : ins.getOutputReg() | needreg)
				{
					pressure++;
					bp.maxPressure = std::max(bp.maxPressure, pressure);
				}
				curIns++;
			}
		}
	}

	void RegisterAllocatorSSA::computeVarOp()
	{
		varOp.resize(nVar);
		func.inBlock->traverse([this](Block *block) -> bool
		{
			for (auto &ins : block->instructions())
			{
				for (Operand *operand : ins.getOutputReg())
				{
					if (operand->val == Operand::InvalidID)
						continue;
					assert(operand->isReg());
					assert(varOp[operand->val].type == Operand::empty || varOp[operand->val].size() == operand->size());
					varOp[operand->val] = *operand;
					varOp[operand->val].pregid = -1;
				}
			}
			return true;
		});
	}

	void RegisterAllocatorSSA::computeVarGroup()
	{
		/*
			Due to the transforms on SSA form, some values of a phi-node may live in the same time.
			We need to manually add some copy instructions to ensure all values related to a phi-node can be put into one memory address.
		*/
		analysisLiveness();
		buildInterferenceGraph();
		auto copySrc = [this](std::pair<Operand, std::weak_ptr<Block>> &src)
		{
			std::shared_ptr<Block> pred = src.second.lock();

			size_t newVar = nVar++;
			Operand op = RegSize(newVar, src.first.size());
			ifGraph.emplace_back();
			ifGraph.back().root = newVar;
			assert(ifGraph.size() == nVar);

			varOp.push_back(op);
			assert(varOp.size() == nVar);

			pred->ins.insert(std::prev(pred->ins.end()), IR(op, Move, src.first));
			property[pred.get()].definedVar.insert(newVar);
			src.first = op;
		};
		func.inBlock->traverse([&copySrc, this](Block *block) -> bool
		{
			for (auto &kv : block->phi)
			{
				Block::PhiIns &phi = kv.second;
				assert(phi.dst.isReg());
				assert(phi.dst.size() == varOp[phi.dst.val].size());

				std::map<size_t, size_t> vtxIndex;	//vreg id -> vertex id in MaxClique graph
				std::vector<std::vector<decltype(&phi.srcs[0])>> regSrc;	//vertex id -> src in phi insn
				for (auto &src : phi.srcs)
				{
					if (!src.first.isReg())
					{
						if (src.first.type != Operand::empty)
							copySrc(src);
						continue;
					}
					assert(src.first.size() == phi.dst.size());
					assert(src.first.size() == varOp[src.first.val].size());
					
					if (ifGraph[findVertexRoot(src.first.val)].neighbor.count(findVertexRoot(phi.dst.val)))
						copySrc(src);
					else
					{
						size_t idx;
						if (vtxIndex.count(src.first.val))
							idx = vtxIndex[src.first.val];
						else
						{
							idx = vtxIndex.size();
							vtxIndex[src.first.val] = idx;
							regSrc.emplace_back();
						}
						regSrc[idx].push_back(&src);
					}
				}
				
				MaxClique solver(vtxIndex.size());
				for (auto iterI = vtxIndex.cbegin(); iterI != vtxIndex.cend(); ++iterI)
					for (auto iterJ = std::next(iterI); iterJ != vtxIndex.cend(); ++iterJ)
					{
						size_t rootI = findVertexRoot(iterI->first);
						size_t rootJ = findVertexRoot(iterJ->first);
						if (!ifGraph[rootI].neighbor.count(rootJ) && !ifGraph[rootJ].neighbor.count(rootI))
							solver.link(iterI->second, iterJ->second);
					}
				std::set<size_t> setMaxClique;
				for (size_t vtx : solver.findMaxClique())
					setMaxClique.insert(vtx);
				for (size_t i = 0; i < regSrc.size(); i++)
				{
					if (!setMaxClique.count(i))
					{
						for(auto src : regSrc[i])
							copySrc(*src);
					}
				}

				std::set<size_t> vertices;
				vertices.insert(phi.dst.val);
				for (auto &src : phi.srcs)
				{
					if (!src.first.isReg())
						continue;
					vertices.insert(src.first.val);
				}
				mergeVertices(vertices);
			}
			return true;
		});

		std::map<size_t, size_t> groupID;
		std::map<size_t, std::set<size_t>> member;
		for (size_t i = 0; i < nVar; i++)
		{
			size_t root = findVertexRoot(i);
			assert(varOp[i].type == varOp[root].type);
			size_t gid;
			if (!groupID.count(root))
			{
				gid = groupID.size() + nVar;
				groupID[root] = gid;
			}
			else
				gid = groupID[root];
			varGroup.push_back(gid);
			member[gid].insert(i);
		}
	}

	void RegisterAllocatorSSA::computeExternalVar()
	{
		for (auto iter = func.inBlock->ins.cbegin(); iter != func.inBlock->ins.cend();)
		{
			if (iter->oper == ExternalVar)
			{
				assert(iter->dst.isReg() && needreg(iter->dst));
				externalVarHint[iter->dst.val] = iter->src1;
				iter = func.inBlock->ins.erase(iter);
			}
			else
				++iter;
		}
	}

	void RegisterAllocatorSSA::spillRegister()
	{
		func.inBlock->traverse_rev_postorder([this](Block *block) -> bool
		{
			property[block].visited = true;
			bool usualBlock = true;
			for (Block *pred : block->preds)
			{
				if (!property[pred].visited)
				{
					usualBlock = false;
					break;
				}
			}
			if (!usualBlock)
				initLoopHeader(block);
			else
				initUsualBlock(block);

			spillRegisterBlock(block);
			return true;
		});
		func.inBlock->traverse([this](Block *block) -> bool
		{
			std::map<Block *, std::map<size_t, size_t>> mapPhiSrc;	//pred -> { phi dst -> phi src }
			std::map<Block *, std::map<size_t, size_t>> mapPhiDst;	//pred -> { phi src -> phi dst }
			for (auto &kv : block->phi)
			{
				for (auto &src : kv.second.srcs)
				{
					if (!src.first.isReg())
						continue;
					mapPhiSrc[src.second.lock().get()][kv.first] = src.first.val;
					mapPhiDst[src.second.lock().get()][src.first.val] = kv.first;
				}
			}
			for (Block *pred : block->preds)
			{
				auto position = std::prev(pred->ins.end());
				for (size_t vregPred : property[pred].Wexit)
				{
					size_t vreg = vregPred;
					if (mapPhiDst[pred].count(vregPred))
						vreg = mapPhiDst[pred][vregPred];
					if (!property[block].Wentry.count(vreg))
						pred->ins.insert(position, getSpillInsn(vregPred));
				}
				for (size_t vreg : property[block].Wentry)
				{
					size_t vregPred = vreg;
					if (block->phi.count(vreg))
					{
						if (!mapPhiSrc[pred].count(vreg))
							continue;
						vregPred = mapPhiSrc[pred][vreg];
					}
					if (!property[pred].Wexit.count(vregPred))
						pred->ins.insert(position, getLoadInsn(vregPred));
				}
			}

			for (auto iter = block->phi.cbegin(); iter != block->phi.cend();)
			{
				if (!property[block].Wentry.count(iter->first))
					block->phi.erase(iter++);
				else
					++iter;
			}

			assert(property[block].Wentry.size() <= phyReg.size());
			assert(property[block].Wexit.size() <= phyReg.size());
#if defined(_DEBUG) && !defined(NDEBUG)
			std::stringstream ss;
			ss << "Wentry: " << std::endl;
			for (size_t i : property[block].Wentry)
				ss << i << " ";
			ss << std::endl;
			ss << "Wexit: " << std::endl;
			for (size_t i : property[block].Wexit)
				ss << i << " ";
			ss << std::endl;
			block->dbgInfo += "\n" + ss.str();
#endif

			return true;
		});
	}

	void RegisterAllocatorSSA::spillRegisterBlock(Block *block)
	{
		std::map<size_t, std::stack<ssize_t>> varUse;	//vreg id -> insn id of use (next use distance at the begin of the block)

		for (auto &kv : property[block].nextUseEnd)
			varUse[kv.first].push(kv.second + block->ins.size());

		ssize_t curInsn = block->ins.size() - 1;
		for (auto iter = block->ins.rbegin(); iter != block->ins.rend(); ++iter)
		{
			for (Operand *operand : iter->getInputReg() | needreg)
			{
				//if (varUse[operand->val].empty() || varUse[operand->val].top() != curInsn)
				varUse[operand->val].push(curInsn);
			}
			curInsn--;
		}

		auto cmpVarUse = [&varUse](size_t a, size_t b) -> bool
		{
			if (varUse[a].empty())
				return false;
			if (varUse[b].empty())
				return true;
			return varUse[a].top() < varUse[b].top();
		};

		std::vector<size_t> W;
		for (size_t vreg : property[block].Wentry)
			W.push_back(vreg);
		std::make_heap(W.begin(), W.end(), cmpVarUse);

		auto limitReg = [&W, &cmpVarUse, &varUse, block, this](size_t maxReg, std::list<Instruction>::iterator pos)
		{
			while (W.size() > maxReg)
			{
				std::pop_heap(W.begin(), W.end(), cmpVarUse);
				if(!varUse[W.back()].empty())
					block->ins.insert(pos, getSpillInsn(W.back()));
				W.pop_back();
			}
		};

		auto eliminateUnusedReg = [&W, &varUse, &cmpVarUse]()
		{
			while (!W.empty() && varUse[W.front()].empty())
			{
				std::pop_heap(W.begin(), W.end(), cmpVarUse);
				W.pop_back();
			}
		};

		curInsn = 0;
		size_t remainRegister = phyReg.size();
		std::map<int, Operand> lastLock;
		std::map<int, size_t> manuallyAllocVar;	//preg id -> var id,  vars that manually assigned to locked register
		for (auto iter = block->ins.begin(); iter != block->ins.end(); ++iter, curInsn++)
		{
			if (isSpill(*iter) || isReload(*iter))
				continue;
			if (iter->oper == LockReg)
			{
				assert(remainRegister == phyReg.size());
				remainRegister -= iter->paramExt.size();
				for (Operand &op : iter->paramExt)
					lastLock[op.pregid] = op;
				continue;
			}
			if (iter->oper == UnlockReg)
			{
				remainRegister = phyReg.size();
				for (auto &kv : manuallyAllocVar)
				{
					W.push_back(kv.second);
					std::push_heap(W.begin(), W.end(), cmpVarUse);
				}
				for (auto &kv : lastLock)
					iter->paramExt.push_back(kv.second);
				manuallyAllocVar.clear();
				lastLock.clear();
				continue;
			}
			assert(iter->oper != ExternalVar);

			if (iter->oper == MoveToRegister)
			{
				std::vector<Operand> dst, src;
				std::vector<Operand *> input = iter->getInputReg();
				std::vector<size_t> reload;
				size_t cntPlaceholder = 0;
				for (Operand *operand : input)
				{
					//assert(operand->val != Operand::InvalidID);
					assert(operand->pregid != -1);
					if (operand->val == Operand::InvalidID)
						cntPlaceholder++;
					else if (std::find(W.begin(), W.end(), operand->val) == W.end())
					{
						reload.push_back(operand->val);
						W.push_back(operand->val);
						std::push_heap(W.begin(), W.end(), cmpVarUse);
					}

					if(operand->val != Operand::InvalidID)
						operand->setSize(varOp[operand->val].size());
					dst.push_back(*operand);
					src.push_back(operand->clone().setPRegID(-1));
				}
				limitReg(remainRegister - cntPlaceholder, iter);
				for (Operand *operand : input | needreg)
					varUse[operand->val].pop();
				std::make_heap(W.begin(), W.end(), cmpVarUse);
				for (size_t vreg : reload)
					block->ins.insert(iter, getLoadInsn(vreg));

				eliminateUnusedReg();
				for (size_t vreg : W)
				{
					if (std::find_if(input.begin(), input.end(), [vreg](Operand *operand) { return operand->val == vreg; }) == input.end())
					{
						dst.push_back(varOp[vreg]);
						src.push_back(varOp[vreg]);
					}
				}
				*iter = IRParallelMove(dst, src);
				continue;
			}

			std::vector<size_t> reload;
			size_t nOperandLastUse = 0;
			for (Operand *operand : iter->getInputReg() | needreg)
			{
				if (std::find(W.begin(), W.end(), operand->val) == W.end())
				{
					reload.push_back(operand->val);
					W.push_back(operand->val);
					std::push_heap(W.begin(), W.end(), cmpVarUse);
				}
				if (varUse[operand->val].size() == 1)
					nOperandLastUse++;
			}
			limitReg(remainRegister, iter);
			
			std::vector<size_t> outRegValid;
			for (Operand *operand : iter->getOutputReg() | hasreg)
			{
				if (operand->pregid != -1 && lastLock.count(operand->pregid))
					manuallyAllocVar[operand->pregid] = operand->val;
				else
					outRegValid.push_back(operand->val);
			}
			limitReg(remainRegister - outRegValid.size() + nOperandLastUse, iter);

			for (Operand *operand : iter->getInputReg() | needreg)
			{
				assert(varUse[operand->val].top() == curInsn);
				varUse[operand->val].pop();
			}
			std::make_heap(W.begin(), W.end(), cmpVarUse);

			for (size_t vreg : outRegValid)
			{
				if (vreg == Operand::InvalidID)
					continue;
				W.push_back(vreg);
				std::push_heap(W.begin(), W.end(), cmpVarUse);
			}
			for (size_t vreg : reload)		//reload instruction must be put after spilling instruction
				block->ins.insert(iter, getLoadInsn(vreg));
		}

		for (size_t vreg : W)
			if (property[block].nextUseEnd.count(vreg))
				property[block].Wexit.insert(vreg);
	}

	Instruction RegisterAllocatorSSA::getLoadInsn(size_t regid)
	{
		Operand op = RegPtr(varGroup[regid]);
		op.noreg = true;
		return IR(varOp[regid], Load, op);
	}

	Instruction RegisterAllocatorSSA::getSpillInsn(size_t regid)
	{
		Operand op = RegPtr(varGroup[regid]);
		op.noreg = true;
		return IRStore(varOp[regid], op);
	}

	void RegisterAllocatorSSA::initLoopHeader(Block *block)
	{
		std::set<size_t> alive, cand;
		std::vector<size_t> livethrough;
		for (auto &kv : property[block].nextUseBegin)
			alive.insert(kv.first);

		size_t maxPressure = 0;
		std::set<Block *> tmp({ block });
		for (Block *blkBody : (property[block].loopBody.empty() ? tmp : property[block].loopBody))
		{
			maxPressure = std::max(maxPressure, property[blkBody].maxPressure);
			for (auto &ins : blkBody->instructions())
			{
				for (Operand *operand : ins.getInputReg())
				{
					if (operand->val == Operand::InvalidID)
						continue;
					if (alive.count(operand->val))
						cand.insert(operand->val);
				}
			}
		}

		for (size_t vreg : alive)
			if (!cand.count(vreg))
				livethrough.push_back(vreg);

		if (cand.size() < phyReg.size())
		{
			for (size_t vreg : cand)
				property[block].Wentry.insert(vreg);
			if (maxPressure - livethrough.size() < phyReg.size())
			{
				size_t freeReg = phyReg.size() + livethrough.size() - maxPressure;
				std::sort(livethrough.begin(), livethrough.end(),
					[this, block](size_t a, size_t b) { return property[block].nextUseBegin[a] < property[block].nextUseBegin[b]; });
				for (size_t i = 0; i < freeReg && i < livethrough.size(); i++)
					property[block].Wentry.insert(livethrough[i]);
			}
		}
		else
		{
			std::vector<size_t> vCand;
			for (size_t vreg : cand)
				vCand.push_back(vreg);
			std::sort(vCand.begin(), vCand.end(),
				[this, block](size_t a, size_t b) { return property[block].nextUseBegin[a] < property[block].nextUseBegin[b]; });
			for (size_t i = 0; i < phyReg.size(); i++)
				property[block].Wentry.insert(vCand[i]);
		}
	}

	void RegisterAllocatorSSA::initUsualBlock(Block *block)
	{
		std::map<size_t, size_t> phiGroup;	//group id -> phi reg id
		for (auto &kv : block->phi)
			phiGroup[varGroup[kv.first]] = kv.first;

		std::map<size_t, size_t> freq;
		std::set<size_t> cand;
		size_t take = 0;
		for (Block *pred : block->preds)
		{
			for (size_t vreg : property[pred].Wexit)
			{
				size_t gid = varGroup[vreg];
				if (phiGroup.count(gid))
				{
					cand.insert(phiGroup[gid]);
					freq[phiGroup[gid]]++;
				}
				else
				{
					cand.insert(vreg);
					freq[vreg]++;
				}
			}
		}
		for (auto &kv : freq)
		{
			if (kv.second == block->preds.size())
			{
				cand.erase(kv.first);
				property[block].Wentry.insert(kv.first);
				take++;
			}
		}

		if (take < phyReg.size())
		{
			std::vector<size_t> vCand;
			for (size_t vreg : cand)
				vCand.push_back(vreg);
			std::sort(vCand.begin(), vCand.end(),
				[this, block](size_t a, size_t b) { return property[block].nextUseBegin[a] < property[block].nextUseBegin[b]; });
			for (size_t i = 0; i < phyReg.size() - take && i < vCand.size(); i++)
				property[block].Wentry.insert(vCand[i]);
		}
	}

	bool RegisterAllocatorSSA::isSpill(const Instruction &insn)
	{
		return insn.oper == Store && insn.src1.isReg() && insn.src1.val >= nVar;
	}

	bool RegisterAllocatorSSA::isReload(const Instruction &insn)
	{
		return insn.oper == Load && insn.src1.isReg() && insn.src1.val >= nVar;
	}

	void RegisterAllocatorSSA::eliminateSpillCode()
	{
		std::set<size_t> spilled;
		for (auto &kv : externalVarHint)
			spilled.insert(kv.first);
		eliminateSpillCode(0, spilled);
	}

	void RegisterAllocatorSSA::eliminateSpillCode(size_t idx, std::set<size_t> &spilled)
	{
		Block *block = vBlock[idx];
		for (auto iter = block->ins.cbegin(); iter != block->ins.cend();)
		{
			if (isSpill(*iter))
			{
				if (spilled.count(iter->dst.val))
				{
					iter = block->ins.erase(iter);
					continue;
				}
				spilled.insert(iter->dst.val);
			}
			++iter;
		}
		for (size_t child : dtree.getDomChildren(idx))
			eliminateSpillCode(child, spilled);
		for (auto &ins : block->ins)
			if(isSpill(ins))
				spilled.erase(ins.dst.val);
	}

	void RegisterAllocatorSSA::insertAllocateCode()
	{
		std::set<size_t> allocated;
		func.inBlock->traverse([&allocated, this](Block *block) -> bool
		{
			for (auto &ins : block->ins)
				if (isSpill(ins) || isReload(ins))
				{
					if (allocated.count(varGroup[ins.dst.val]))
						continue;
					Instruction insn = IR(ins.src1, Allocate, ImmPtr(ins.dst.size()), ImmPtr(ins.dst.size()));
					if (externalVarHint.count(ins.dst.val))
						insn.paramExt.push_back(externalVarHint[ins.dst.val]);
					func.inBlock->ins.push_front(insn);
					allocated.insert(varGroup[ins.dst.val]);
				}
			return true;
		});
	}

	void RegisterAllocatorSSA::reconstructSSA()
	{
		SSAReconstructor reconstructor(func);
		reconstructor.preprocess();
		reconstructor.reconstructAuto();
	}

	void RegisterAllocatorSSA::analysisLiveness()
	{
		for (auto &kv : property)
		{
			kv.second.liveIn.clear();
			kv.second.liveOut.clear();
		}

		std::queue<Block *> Q;
		std::map<Block *, std::set<size_t>> updatedVar;
		auto newLiveOut = [&Q, &updatedVar, this](Block *block, size_t var)
		{
			BlockProperty &bp = property[block];
			if (!bp.liveOut.count(var))
			{
				bp.liveOut.insert(var);
				if (updatedVar[block].empty())
					Q.push(block);
				updatedVar[block].insert(var);
			}
		};
		auto updatedLiveIn = [&newLiveOut](Block *block, size_t var)
		{
			for (Block *pred : block->preds)
				newLiveOut(pred, var);
		};
		func.inBlock->traverse([&newLiveOut, &updatedLiveIn, this](Block *block) -> bool
		{
			BlockProperty &bp = property[block];
			for (auto iter = block->ins.rbegin(); iter != block->ins.rend(); ++iter)
			{
				for (Operand *operand : iter->getOutputReg() | needreg)
					bp.liveIn.erase(operand->val);
				for (Operand *operand : iter->getInputReg() | needreg)
					bp.liveIn.insert(operand->val);
			}
			for (auto &kv : block->phi)
			{
				bp.liveIn.erase(kv.first);
				for (auto &src : kv.second.srcs)
				{
					if (!src.first.isReg())
						continue;
					std::shared_ptr<Block> pred = src.second.lock();
					newLiveOut(pred.get(), src.first.val);
				}
			}
			for (size_t var : bp.liveIn)
				updatedLiveIn(block, var);
			return true;
		});
		while (!Q.empty())
		{
			Block *block = Q.front();
			Q.pop();
			BlockProperty &bp = property[block];
			std::vector<size_t> vars;
			for (size_t var : updatedVar[block])
				vars.push_back(var);
			updatedVar[block].clear();
			for (size_t var : vars)
			{
				if (!bp.definedVar.count(var) && !bp.usedVar.count(var) && !block->phi.count(var))
				{
					bp.liveIn.insert(var);
					updatedLiveIn(block, var);
				}
			}
		}

#if defined(_DEBUG) && !defined(NDEBUG)
		func.inBlock->traverse([this](Block *block) -> bool
		{
			std::stringstream ss;
			ss << "Idx: " << property[block].idx << std::endl;
			ss << "Live In: ";
			for (size_t vreg : property[block].liveIn)
				ss << vreg << ", ";
			ss << std::endl;
			ss << "Live Out: ";
			for (size_t vreg : property[block].liveOut)
				ss << vreg << ", ";
			ss << std::endl;
			block->dbgInfo = ss.str();
			return true;
		});
		

		if (!property[func.inBlock.get()].liveIn.empty())
			std::cerr << "Non-empty live in!" << std::endl;
#endif
		//assert(property[func.inBlock.get()].liveIn.empty());
	}

	void RegisterAllocatorSSA::buildInterferenceGraph()
	{
		ifGraph.clear();
		ifGraph.resize(nVar);
		for (size_t i = 0; i < ifGraph.size(); i++)
			ifGraph[i].root = i;

		func.inBlock->traverse([this](Block *block) -> bool
		{
			BlockProperty &bp = property[block];
			std::map<size_t, std::stack<ssize_t>> uses;
			for (size_t vreg : bp.liveOut)
				uses[vreg].push(-1);
			ssize_t curInsn = block->ins.size() - 1;
			for (auto iter = block->ins.rbegin(); iter != block->ins.rend(); ++iter)
			{
				for (Operand *operand : iter->getInputReg() | needreg)
				{
					uses[operand->val].push(curInsn);
				}
				curInsn--;
			}
			
			std::shared_ptr<std::set<int>> forbiddenReg;
			std::set<size_t> live = bp.liveIn;
			auto interfere = [&live, &forbiddenReg, this](size_t vreg, std::shared_ptr<std::set<int>> altForbiddenReg)
			{
				assert(!live.count(vreg));
				//assert(live.size() <= phyReg.size() - (forbiddenReg ? forbiddenReg->size() : 0));
				for (size_t neighbor : live)
				{
					ifGraph[vreg].neighbor.insert(neighbor);
					ifGraph[neighbor].neighbor.insert(vreg);
				}
				if (!altForbiddenReg)
					ifGraph[vreg].forbiddenReg = forbiddenReg;
				else
					ifGraph[vreg].forbiddenReg = altForbiddenReg;
			};
			for (auto &kv : block->phi)
			{
				interfere(kv.first, nullptr);
				if (!uses[kv.first].empty())
					live.insert(kv.first);
			}
			curInsn = 0;
			for (auto &ins : block->ins)
			{
				if (ins.oper == LockReg)
				{
					forbiddenReg.reset(new std::set<int>());
					for (Operand operand : ins.paramExt)
					{
						assert(operand.isReg() && operand.val == Operand::InvalidID);
						forbiddenReg->insert(operand.pregid);
					}
					continue;
				}
				if (ins.oper == UnlockReg)
				{
					forbiddenReg.reset();
					continue;
				}

				for (Operand *operand : ins.getInputReg() | needreg)
				{
					uses[operand->val].pop();
					if (uses[operand->val].empty())
						live.erase(operand->val);
				}

				std::shared_ptr<std::set<int>> alternative;
				if (forbiddenReg)
					alternative.reset(new std::set<int>(*forbiddenReg));
				else
					alternative.reset(new std::set<int>());
				bool enableAlternative = false;
				for (Operand *operand : ins.getOutputReg() | hasreg)
				{
					if (operand->pregid != -1)
					{
						alternative->insert(operand->pregid);
						enableAlternative = true;
					}
				}

				for (Operand *operand : ins.getOutputReg() | needreg)
				{
					if (operand->pregid != -1)
					{
						std::shared_ptr<std::set<int>> tmp(new std::set<int>());
						for (int preg : phyReg)
							if (preg != operand->pregid)
								tmp->insert(preg);
						interfere(operand->val, tmp);
					}
					else
					{
						if (!enableAlternative)
							interfere(operand->val, nullptr);
						else
							interfere(operand->val, alternative);
					}
					//if (!uses[operand->val].empty())
					live.insert(operand->val);
				}
				for (Operand *operand : ins.getOutputReg() | needreg)
				{
					if (uses[operand->val].empty())
						live.erase(operand->val);
				}
				curInsn++;
			}
			return true;
		});
	}

	void RegisterAllocatorSSA::allocateRegister()
	{
		allocateRegisterDFS(0);
	}

	void RegisterAllocatorSSA::allocateRegisterDFS(size_t idx)
	{
		Block *block = vBlock[idx];
		for (auto &ins : block->instructions())
		{
			std::vector<int> prefer;
			if (ins.hint == InstructionBase::PreferCorrespondingOperand)
			{
				std::vector<Operand *> input = ins.getInputReg(), output = ins.getOutputReg();
				assert(input.size() >= output.size());
				for (size_t i = 0; i < output.size(); i++)
				{
					if (output[i]->val == Operand::InvalidID || output[i]->noreg)
						continue;
					prefer.clear();
					if (input[i]->pregid != -1)
						prefer.push_back(input[i]->pregid);
					else if (input[i]->val != Operand::InvalidID && !input[i]->noreg && ifGraph[input[i]->val].preg != -1)
						prefer.push_back(ifGraph[input[i]->val].preg);
					ifGraph[output[i]->val].preg = chooseRegister(output[i]->val, prefer);
				}
				continue;
			}
			if (ins.hint == InstructionBase::PreferAnyOfOperands || ins.hint == InstructionBase::PreferOperands)
			{
				for (Operand *operand : ins.getInputReg())
				{
					if (operand->pregid != -1)
						prefer.push_back(operand->pregid);
					else if (operand->val != Operand::InvalidID && !operand->noreg && ifGraph[operand->val].preg != -1)
						prefer.push_back(ifGraph[operand->val].preg);
				}
			}
			for (Operand *operand : ins.getOutputReg() | needreg)
				ifGraph[operand->val].preg = chooseRegister(operand->val, prefer);
		}

		for (size_t child : dtree.getDomChildren(idx))
			allocateRegisterDFS(child);
	}

	int RegisterAllocatorSSA::chooseRegister(size_t vreg, const std::vector<int> &prefer)
	{
		auto testRegister = [vreg, this](int preg)
		{
			if (ifGraph[vreg].forbiddenReg && ifGraph[vreg].forbiddenReg->count(preg))
				return false;
			for (size_t neighbor : ifGraph[vreg].neighbor)
				if (ifGraph[neighbor].preg == preg)
					return false;
			return true;
		};
		static int lastReg = phyReg[phyReg.size() - 1];
		for (int preg : prefer)
			if (testRegister(preg))
				return lastReg = preg;
		
		bool flag = false;
		for (size_t i = 0; i < phyReg.size(); i++)
		{
			if (lastReg == phyReg[i])
			{
				for (size_t j = 1; j <= phyReg.size(); j++)
				{
					int preg = phyReg[(j + i) % phyReg.size()];
					if (testRegister(preg))
						return lastReg = preg;
				}
				flag = true;
				break;
			}
		}
		if (!flag)
		{
			for (int preg : phyReg)
				if (testRegister(preg))
					return lastReg = preg;
		}
		assert(false);
		return -1;
	}

	int RegisterAllocatorSSA::getPReg(Operand operand)
	{
		assert(!operand.noreg);
		if (operand.pregid != -1)
			return operand.pregid;
		assert(operand.val != Operand::InvalidID);
		return ifGraph[findVertexRoot(operand.val)].preg;
	}

	void RegisterAllocatorSSA::coalesce()
	{
		struct OUcmp
		{
			bool operator()(const std::shared_ptr<OptimUnit> &a, const std::shared_ptr<OptimUnit> &b) const
			{
				assert(a && b);
				return *a < *b;
			}
		};
		func.inBlock->traverse([this](Block *block) -> bool
		{
			for (auto &ins : block->instructions())
			{
				std::priority_queue<std::shared_ptr<OptimUnit>, std::vector<std::shared_ptr<OptimUnit>>, OUcmp> pq;
				auto runOptim = [&pq]()
				{
					while (!pq.empty())
					{
						auto OU = pq.top();
						pq.pop();
						int ret = OU->work();
						if (ret > 0)
							break;
						if (ret == 0)
							pq.push(OU);
					}
				};
				auto addOptim = [&pq, this](Operand key, Operand target)
				{
					if (target.pregid != -1 || ifGraph[target.val].pinned)
						pq.emplace(new OptimUnitSingle(*this, key.val, getPReg(target)));
					else if (key.pregid != -1 || ifGraph[key.val].pinned)
						pq.emplace(new OptimUnitSingle(*this, target.val, getPReg(key)));
					else
					{
						assert(target.val != Operand::InvalidID);
						OptimUnitOne OU(*this, key.val, target.val);
						for (int preg : phyReg)
						{
							if (ifGraph[key.val].forbiddenReg && ifGraph[key.val].forbiddenReg->count(preg))
								continue;
							if (ifGraph[target.val].forbiddenReg && ifGraph[target.val].forbiddenReg->count(preg))
								continue;
							OptimUnitOne tmp = OU;
							tmp.targetRegister = preg;
							pq.emplace(new OptimUnitOne(std::move(tmp)));
						}
					}
				};
				auto clearOptim = [&pq]()
				{
					while (!pq.empty())
						pq.pop();
				};
				if (ins.hint == InstructionBase::NoPrefer)
					continue;
				if (ins.hint == InstructionBase::PreferAnyOfOperands)
				{
					for (Operand *opOut : ins.getOutputReg() | hasreg)
					{
						clearOptim();
						bool flag = false;
						for (Operand *opIn : ins.getInputReg() | hasreg)
						{
							if (getPReg(*opOut) == getPReg(*opIn))
							{
								if (needreg(*opOut) && needreg(*opIn))
									mergeVertices({ opOut->val, opIn->val });
								else
								{
									if (opOut->val != Operand::InvalidID)
										ifGraph[opOut->val].pinned = true;
									if (opIn->val != Operand::InvalidID)
										ifGraph[opIn->val].pinned = true;
								}
								flag = true;
								break;
							}
							addOptim(*opOut, *opIn);
						}
						if (!flag)
							runOptim();
					}
				}
				else if (ins.hint == InstructionBase::PreferCorrespondingOperand)
				{
					auto input = ins.getInputReg();
					auto output = ins.getOutputReg();
					assert(output.size() <= input.size());
					for (size_t i = 0; i < output.size(); i++)
					{
						Operand out = *output[i], in = *input[i];
						if (!hasreg(out) || !hasreg(in))
							continue;
						clearOptim();
						if (getPReg(out) == getPReg(in))
						{
							if (needreg(out) && needreg(in))
								mergeVertices({ out.val, in.val });
							else
							{
								if (needreg(out))
									ifGraph[out.val].pinned = true;
								if (needreg(in))
									ifGraph[in.val].pinned = true;
							}
							continue;
						}
						addOptim(out, in);
						runOptim();
					}
				}
				else if (ins.hint == InstructionBase::PreferOperands)
				{
					for (Operand *opOut : ins.getOutputReg() | needreg)
					{
						clearOptim();
						std::vector<size_t> srcVar;
						for (Operand *opIn : ins.getInputReg() | needreg)
							srcVar.push_back(opIn->val);
						OptimUnitAll OU(*this, opOut->val, srcVar);
						for (int preg : phyReg)
						{
							if (ifGraph[opOut->val].forbiddenReg && ifGraph[opOut->val].forbiddenReg->count(preg))
								continue;
							OptimUnitAll tmp = OU;
							tmp.targetRegister = preg;
							pq.emplace(new OptimUnitAll(std::move(tmp)));
						}
						runOptim();
					}
				}
			}
			return true;
		});
	}

	int RegisterAllocatorSSA::OptimUnit::work()
	{
		if (S.size() < minCand || !S.count(keyVertex))
			return -1;
		cand.clear();
		changedReg.clear();
		for (size_t u : join<const size_t>(std::array<size_t, 1>{ keyVertex }, S))
		{
			if (cand.count(u))
				continue;
			ssize_t ret = adjust(u, targetRegister);
			if (u == keyVertex && ret != -1)
				return -1;
			if (ret == u || ret == keyVertex)
			{
				cand.clear(); changedReg.clear();
				fail(u);
				return S.size() >= minCand ? 0 : -1;
			}
			if (ret != -1)
			{
				cand.clear(); changedReg.clear();
				conflict(u, ret);
				return S.size() >= minCand ? 0 : -1;
			}
			cand.insert(u);
		}
		assert(cand.size() >= minCand);
		for (auto &kv : changedReg)
		{
			assert(!allocator.ifGraph[kv.first].forbiddenReg || !allocator.ifGraph[kv.first].forbiddenReg->count(kv.second));
			assert(!allocator.ifGraph[kv.first].pinned);
			allocator.ifGraph[kv.first].preg = kv.second;
			//allocator.ifGraph[kv.first].pinned = true;
		}
		apply();
		return 1;
	}

	void RegisterAllocatorSSA::OptimUnit::apply()
	{
		allocator.mergeVertices(S);
		/*for (size_t var : cand)
			allocator.ifGraph[var].pinned = true;*/
	}
	//What this function actually do:
	// In a induced subgraph of G: G'(V', E') satisfying for each v in V': color(v) in { old_color(src), new_color(src) } 
	//  for each v in V': if v is connected to src, color(v) = !color(v)
	ssize_t RegisterAllocatorSSA::OptimUnit::adjust(size_t src)
	{
		if (allocator.ifGraph[src].preg == targetRegister)
			return -1;
		struct stkInfo
		{
			size_t idx;
			int oldReg;
			std::set<size_t>::const_iterator iter;
		};
		std::stack<stkInfo> stk;
		stk.push(stkInfo{ src, getCurrentReg(src), allocator.ifGraph[src].neighbor.cbegin() });
		if (allocator.ifGraph[src].pinned || (allocator.ifGraph[src].forbiddenReg && allocator.ifGraph[src].forbiddenReg->count(targetRegister)))
			return src;
		changedReg[src] = targetRegister;
		while (!stk.empty())
		{
			stkInfo context = stk.top();
			stk.pop();
			auto &cur = allocator.ifGraph[context.idx];
			while (context.iter != cur.neighbor.cend())
			{
				auto &next = allocator.ifGraph[*context.iter];
				if (next.preg == cur.preg)
				{
					if (next.pinned || (next.forbiddenReg && next.forbiddenReg->count(context.oldReg)))
						return src;
					if (cand.count(*context.iter))
						return *context.iter;
					changedReg[*context.iter] = context.oldReg;
					//next.preg = context.oldReg;
					auto oldIter = context.iter++;
					stk.push(context);
					stk.push(stkInfo{ *oldIter, cur.preg, next.neighbor.cbegin() });
				}
				else
					++context.iter;
			}
		}
		return -1;
	}

	ssize_t RegisterAllocatorSSA::OptimUnit::adjust(size_t src, int target)
	{
		const auto &G = allocator.ifGraph;
		if (G[src].pinned || (G[src].forbiddenReg && G[src].forbiddenReg->count(target)))
			return src;
		int oldReg = getCurrentReg(src);
		changedReg[src] = target;
		for (size_t neighbor : G[src].neighbor)
		{
			if (getCurrentReg(neighbor) == target)
			{
				if (cand.count(neighbor))
					return neighbor;
				ssize_t ret = adjust(neighbor, oldReg);
				if (ret == neighbor)
					return src;
				if (ret != -1)
					return ret;
			}
		}
		return -1;
	}

	RegisterAllocatorSSA::OptimUnitAll::OptimUnitAll(RegisterAllocatorSSA &allocator, size_t key, const std::vector<size_t> &another) : OptimUnit(allocator, 2, -1)
	{
		keyVertex = allocator.findVertexRoot(key);
		vertices.insert(keyVertex);
		for (size_t var : another)
		{
			var = allocator.findVertexRoot(var);
			if (var == keyVertex)
				continue;
			if (!allocator.ifGraph[keyVertex].neighbor.count(var) && !allocator.ifGraph[var].neighbor.count(keyVertex))
				vertices.insert(var);
		}
		for (auto iterI = vertices.begin(); iterI != vertices.end(); ++iterI)
			for (auto iterJ = std::next(iterI); iterJ != vertices.end(); ++iterJ)
			{
				if (allocator.ifGraph[*iterI].neighbor.count(*iterJ) || allocator.ifGraph[*iterJ].neighbor.count(*iterI))
				{
					edges.insert(std::make_pair(*iterI, *iterJ));
					edges.insert(std::make_pair(*iterJ, *iterI));
				}
			}
		findMaxStableSet();
	}
	void RegisterAllocatorSSA::OptimUnitAll::fail(size_t u)
	{
		vertices.erase(u);
		for (auto iter = edges.cbegin(); iter != edges.cend();)
		{
			if (iter->first == u || iter->second == u)
				iter = edges.erase(iter);
			else
				++iter;
		}
		findMaxStableSet();
	}
	void RegisterAllocatorSSA::OptimUnitAll::conflict(size_t u, size_t v)
	{
		edges.insert(std::make_pair(u, v));
		edges.insert(std::make_pair(v, u));
		findMaxStableSet();
	}
	void RegisterAllocatorSSA::OptimUnitAll::findMaxStableSet()
	{
		assert(vertices.count(keyVertex));
		assert(std::count_if(edges.begin(), edges.end(), [this](const std::pair<size_t, size_t> &e) { return e.first == keyVertex || e.second == keyVertex; }) == 0);
		S.clear();

		std::map<size_t, size_t> graphID;	//original id -> id in MaxClique
		std::vector<size_t> originalID;		//id in MaxClique -> original id
		for (size_t p : vertices)
		{
			graphID[p] = originalID.size();
			originalID.push_back(p);
		}
		MaxClique clique(originalID.size());
		for (size_t i = 0; i < originalID.size(); i++)
			for (size_t j = i+1; j < originalID.size(); j++)
			{
				if (!edges.count(std::make_pair(originalID[i], originalID[j])) &&
					!edges.count(std::make_pair(originalID[j], originalID[i])))
					clique.link(i, j);
			}
		for (size_t t : clique.findMaxClique())
			S.insert(originalID[t]);
	}

	RegisterAllocatorSSA::OptimUnitOne::OptimUnitOne(RegisterAllocatorSSA &allocator, size_t u, size_t v) : OptimUnit(allocator, 2, -1)
	{
		keyVertex = allocator.findVertexRoot(u);
		another = allocator.findVertexRoot(v);
		if (keyVertex != another && !allocator.ifGraph[keyVertex].neighbor.count(another) && !allocator.ifGraph[another].neighbor.count(keyVertex))
			S.insert({ keyVertex, another });
	}

	void RegisterAllocatorSSA::destructSSA()
	{
		func.inBlock->traverse([](Block *block) -> bool
		{
			std::map<Block *, std::pair<std::vector<Operand>, std::vector<Operand>>> pMoveInsn;	//pred -> (dst[], src[])
			for (auto &kv : block->phi)
			{
				assert(needreg(kv.second.dst));
				for (auto &src : kv.second.srcs)
					if (src.first.isReg())
					{
						assert(needreg(src.first));
						std::shared_ptr<Block> pred = src.second.lock();
						pMoveInsn[pred.get()].first.push_back(kv.second.dst);
						pMoveInsn[pred.get()].second.push_back(src.first);
					}
			}
			for (auto &kv : pMoveInsn)
				kv.first->ins.insert(std::prev(kv.first->ins.end()), IRParallelMove(kv.second.first, kv.second.second));
			block->phi.clear();
			return true;
		});
	}

	void RegisterAllocatorSSA::writeRegInfo()
	{
		func.inBlock->traverse([this](Block *block) -> bool
		{
			for (auto &ins : block->instructions())
				for (Operand *operand : join<Operand *>(ins.getInputReg() | needreg, ins.getOutputReg() | needreg))
				{
					operand->pregid = ifGraph[findVertexRoot(operand->val)].preg;
				}
			return true;
		});
	}

	size_t RegisterAllocatorSSA::findVertexRoot(size_t vtx)
	{
		if (ifGraph[vtx].root == vtx)
			return vtx;
		return ifGraph[vtx].root = findVertexRoot(ifGraph[vtx].root);
	}

	void RegisterAllocatorSSA::mergeVertices(const std::set<size_t> &vtxList)
	{
		std::set<size_t> vertices;
		for (size_t vtx : vtxList)
			vertices.insert(findVertexRoot(vtx));

		if (vertices.size() <= 1)
			return;
		size_t maxNeighbor = 0;
		size_t root = *vertices.begin();
		std::shared_ptr<std::set<int>> forbiddenReg;
		bool flag = false;
		for (size_t vtx : vertices)
		{
			if (ifGraph[vtx].neighbor.size() > maxNeighbor)
			{
				maxNeighbor = ifGraph[vtx].neighbor.size();
				root = vtx;
			}
			if (ifGraph[vtx].forbiddenReg && ifGraph[vtx].forbiddenReg != forbiddenReg)
			{
				if (!forbiddenReg)
					forbiddenReg = ifGraph[vtx].forbiddenReg;
				else
				{
					if (!flag)
					{
						forbiddenReg.reset(new std::set<int>(*forbiddenReg));
						flag = true;
					}
					for (int reg : *ifGraph[vtx].forbiddenReg)
						forbiddenReg->insert(reg);
				}
			}
		}
		for (size_t vtx : vertices)
		{
			if (vtx == root)
				continue;
			assert(ifGraph[vtx].preg == ifGraph[root].preg);
			for (size_t neighbor : ifGraph[vtx].neighbor)
			{
				assert(neighbor != vtx);
				ifGraph[root].neighbor.insert(neighbor);
				ifGraph[neighbor].neighbor.erase(vtx);
				ifGraph[neighbor].neighbor.insert(root);
			}
			ifGraph[vtx].neighbor.clear();
			ifGraph[vtx].forbiddenReg.reset();
			ifGraph[vtx].root = root;
		}
		ifGraph[root].forbiddenReg = forbiddenReg;
	}
}