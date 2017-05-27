#include "common_headers.h"
#include "IR.h"
#include "utils/CycleEquiv.h"
#include "LoopDetector.h"

namespace MxIR
{
	Block::block_ptr::~block_ptr()
	{
		if (ptr)
		{
			//std::cerr << "Destructing " << curBlock << ":" << this << "->" << ptr.get() << std::endl;
			ptr->removePred(iterPred);
		}
	}
	void Block::block_ptr::reset()
	{
		if (ptr)
			ptr->removePred(iterPred);
		ptr.reset();
	}
	Block::block_ptr & Block::block_ptr::operator=(const block_ptr &other)
	{
		(*this) = other.ptr;
		return *this;
	}
	Block::block_ptr & Block::block_ptr::operator=(const std::shared_ptr<Block> &block)
	{
		if (ptr)
			ptr->removePred(iterPred);
		ptr = block;
		if (ptr)
			iterPred = ptr->newPred(curBlock);
		return *this;
	}
	Block::block_ptr & Block::block_ptr::operator=(std::shared_ptr<Block> &&block)
	{
		if (ptr)
			ptr->removePred(iterPred);
		ptr = std::move(block);
		if (ptr)
			iterPred = ptr->newPred(curBlock);
		return *this;
	}

	std::shared_ptr<Block> Block::construct()
	{
		std::shared_ptr<Block> ptr(new Block);
		ptr->self = ptr;
		return ptr;
	}
	void Block::traverse(std::function<bool(Block *)> func)
	{
		std::set<Block *> visited;
		std::queue<Block *> q;
		q.push(this);
		visited.insert(this);
		while (!q.empty())
		{
			Block *cur = q.front();
			q.pop();
			if (!func(cur))
				return;
			if (cur->brTrue && visited.find(cur->brTrue.get()) == visited.end())
			{
				visited.insert(cur->brTrue.get());
				q.push(cur->brTrue.get());
			}
			if (cur->brFalse && visited.find(cur->brFalse.get()) == visited.end())
			{
				visited.insert(cur->brFalse.get());
				q.push(cur->brFalse.get());
			}
		}
	}
	void Block::traverse_preorder(std::function<bool(Block *)> func)
	{
		std::set<Block *> visited;
		std::function<bool(Block *)> dfs;
		dfs = [&func, &dfs, &visited](Block *block) -> bool
		{
			visited.insert(block);
			if (!func(block))
				return false;
			if (block->brTrue && !visited.count(block->brTrue.get()))
				if (!dfs(block->brTrue.get()))
					return false;
			if (block->brFalse && !visited.count(block->brFalse.get()))
				if (!dfs(block->brFalse.get()))
					return false;
			return true;
		};
		dfs(this);
	}
	void Block::traverse_postorder(std::function<bool(Block *)> func)
	{
		std::set<Block *> visited;
		std::function<bool(Block *)> dfs;
		dfs = [&func, &dfs, &visited](Block *block) -> bool
		{
			visited.insert(block);
			if (block->brTrue && !visited.count(block->brTrue.get()))
				if (!dfs(block->brTrue.get()))
					return false;
			if (block->brFalse && !visited.count(block->brFalse.get()))
				if (!dfs(block->brFalse.get()))
					return false;
			if (!func(block))
				return false;
			return true;
		};
		dfs(this);
	}
	void Block::traverse_rev_postorder(std::function<bool(Block *)> func)
	{
		std::vector<Block *> postOrder;
		traverse_postorder([&postOrder](Block *block) { postOrder.push_back(block); return true; });
		for (auto iter = postOrder.rbegin(); iter != postOrder.rend(); ++iter)
			if (!func(*iter))
				return;
	}
	std::list<Block *>::iterator Block::newPred(Block *pred)
	{
		//std::cerr << "link: " << pred << " -> " << this << std::endl;
		preds.push_back(pred);
		return std::prev(preds.end());
	}
	void Block::removePred(std::list<Block *>::iterator iterPred)
	{
		//std::cerr << "cut " << this << std::endl;
		//std::cerr << "cut: " << *iterPred << " -> " << this << std::endl;
		preds.erase(iterPred);
	}

	Function Function::clone()
	{
		std::map<Block *, std::shared_ptr<Block>> mapNewBlock;	// old block -> new block
		inBlock->traverse([&mapNewBlock](Block *block) -> bool
		{
			std::shared_ptr<Block> newBlock = Block::construct();
			newBlock->phi = block->phi;
			newBlock->ins = block->ins;
			newBlock->sigma = block->sigma;
			mapNewBlock[block] = std::move(newBlock);
			return true;
		});
		inBlock->traverse([&mapNewBlock](Block *block) -> bool
		{
			if (block->brTrue)
				mapNewBlock[block]->brTrue = mapNewBlock[block->brTrue.get()];
			if (block->brFalse)
				mapNewBlock[block]->brFalse = mapNewBlock[block->brFalse.get()];
			return true;
		});
		Function ret;
		ret.params = params;
		ret.inBlock = mapNewBlock[inBlock.get()];
		ret.outBlock = mapNewBlock[outBlock.get()];
		return ret;
	}

	void PSTNode::traverse(std::function<void(PSTNode *)> func)
	{
		std::function<void(PSTNode *)> dfs;
		dfs = [&dfs, &func](PSTNode *node)
		{
			for (auto &child : node->children)
				dfs(child.get());
			func(node);
		};
		dfs(this);
	}

	void Function::constructPST()
	{
		std::map<Block *, size_t> blockID;
		std::vector<Block *> vBlocks;
		std::vector<std::pair<Block *, Block *>> vEdges;
		inBlock->traverse_preorder([&](Block *block) -> bool
		{
			block->pstNode.reset();
			blockID[block] = vBlocks.size();
			vBlocks.push_back(block);
			if (block->brTrue)
				vEdges.push_back(std::make_pair(block, block->brTrue.get()));
			if (block->brFalse)
				vEdges.push_back(std::make_pair(block, block->brFalse.get()));	//note that the ids of edges are in dfs order
			return true;
		});

		vEdges.push_back(std::make_pair(outBlock.get(), inBlock.get()));
		CycleEquiv solver(vBlocks.size());
		for (auto &e : vEdges)
			solver.addEdge(blockID[e.first], blockID[e.second]);

		std::vector<std::set<size_t>> result = solver.work();
		std::vector<std::shared_ptr<PSTNode>> nodes;
		std::map<PSTNode *, Block *> outBlockNext;
		for (auto &equClass : result)
		{
			std::vector<size_t> tmp;
			if (*std::prev(equClass.end()) == vEdges.size() - 1)
				tmp.push_back(vEdges.size() - 1);
			for (size_t e : equClass)
				tmp.push_back(e);

			for (auto iter = tmp.begin(); iter != std::prev(tmp.end()); ++iter)
			{
				std::shared_ptr<PSTNode> cur(new PSTNode);
				cur->inBlock = vEdges[*iter].second;
				cur->outBlock = vEdges[*std::next(iter)].first;
				outBlockNext[cur.get()] = vEdges[*std::next(iter)].second;
				cur->inBlock->pstNode = cur;
				cur->outBlock->pstNode = cur;
				cur->blocks.insert(cur->inBlock);
				cur->blocks.insert(cur->outBlock);
				cur->self = cur;
				nodes.emplace_back(std::move(cur));
			}
		}

		std::shared_ptr<PSTNode> root(new PSTNode);

		std::stack<std::shared_ptr<PSTNode>> stkPST;
		std::set<Block *> visited;
		std::function<void(Block *)> dfs;
		stkPST.push(root);
		dfs = [&outBlockNext, &stkPST, &visited, &dfs](Block *block)
		{
			visited.insert(block);
			int flag = block->pstNode.expired() ? 0 :
				stkPST.top() == block->pstNode.lock() ? -1 : 1;

			if (flag == 1)
			{
				std::shared_ptr<PSTNode> curNode = block->pstNode.lock();
				stkPST.top()->children.push_back(curNode);
				curNode->iterParent = std::prev(stkPST.top()->children.end());
				curNode->parent = stkPST.top();
				stkPST.push(block->pstNode.lock());
			}
			else if(flag == 0)
			{
				block->pstNode = stkPST.top();
				stkPST.top()->blocks.insert(block);
			}

			std::shared_ptr<PSTNode> oldTop;
			if (block->brTrue && !visited.count(block->brTrue.get()))
			{
				if (block->brTrue.get() == outBlockNext[stkPST.top().get()])
				{
					oldTop = stkPST.top();
					stkPST.pop();
				}
				dfs(block->brTrue.get());
				if (oldTop)
					stkPST.emplace(std::move(oldTop));
			}
			if (block->brFalse && !visited.count(block->brFalse.get()))
			{
				if (block->brFalse.get() == outBlockNext[stkPST.top().get()])
				{
					oldTop = stkPST.top();
					stkPST.pop();
				}
				dfs(block->brFalse.get());
				if (oldTop)
					stkPST.emplace(std::move(oldTop));
			}

			if (flag == 1)
				stkPST.pop();
		};

		dfs(inBlock.get());
		pstRoot = root;
	}

	void Function::splitProgramRegion()
	{
		std::map<size_t, size_t> maxVer;
		std::vector<Block *> vBlocks;
		inBlock->traverse([&maxVer, &vBlocks, this](Block *block) -> bool
		{
			if(block != outBlock.get())
				vBlocks.push_back(block);
			for (auto &ins : block->instructions())
				for (Operand *operand : join<Operand *>(ins.getInputReg(), ins.getOutputReg()))
					maxVer[operand->val] = std::max(maxVer[operand->val], operand->ver);
			return true;
		});

		LoopDetector detector(*this);
		detector.findLoops();
		auto &loops = detector.getLoops();

		for (Block *block : vBlocks)
		{
			if (loops.count(block))
			{
				auto &loopBody = loops.find(block)->second;
				std::vector<Block *> entryPred;
				for (Block *pred : block->preds)
					if (!loopBody.count(pred))
						entryPred.push_back(pred);

				if (entryPred.size() > 1)
				{
					std::shared_ptr<Block> blockPreheader = Block::construct();
					blockPreheader->ins = { IRJump() };
					blockPreheader->brTrue = block->self.lock();
					for (Block *pred : entryPred)
					{
						if (pred->brTrue.get() == block)
							pred->brTrue = blockPreheader;
						if (pred->brFalse.get() == block)
							pred->brFalse = blockPreheader;
					}

					for (auto iter = block->phi.begin(); iter != block->phi.end();)
					{
						Block::PhiIns upperPhi, remainPhi;
						bool bRedundantRemainPhi = true, bRedundantUpperPhi = true;
						for (auto &src : iter->second.srcs)
						{
							if (!loopBody.count(src.second.lock().get()))
							{
upperPhi.srcs.push_back(src);
if (src.first.isReg())
bRedundantUpperPhi = false;
							}
							else
							{
								remainPhi.srcs.push_back(src);
								assert(src.first.isReg());
								if (src.first.val != iter->second.dst.val || src.first.ver != iter->second.dst.ver)
									bRedundantRemainPhi = false;
							}
						}

						if (bRedundantRemainPhi)
						{
							upperPhi.dst = iter->second.dst;
							blockPreheader->phi[upperPhi.dst.val] = upperPhi;
							iter = block->phi.erase(iter);
						}
						else if (bRedundantUpperPhi)
						{
							remainPhi.dst = iter->second.dst;
							remainPhi.srcs.push_back({ EmptyOperand(), blockPreheader });
							iter->second = remainPhi;
							++iter;
						}
						else
						{
							upperPhi.dst = iter->second.dst;
							upperPhi.dst.ver = ++maxVer[upperPhi.dst.val];
							blockPreheader->phi[upperPhi.dst.val] = upperPhi;
							remainPhi.dst = iter->second.dst;
							remainPhi.srcs.push_back({ upperPhi.dst, blockPreheader });
							iter->second = remainPhi;
							++iter;
						}
					}
				}
				continue;
			}
			if (block->preds.size() > 1)
			{
				auto preds = block->preds;
				std::shared_ptr<Block> tmp = Block::construct();
				tmp->phi = std::move(block->phi);
				tmp->brTrue = block->self.lock();
				tmp->ins = { IRJump() };
				for (Block *pred : preds)
				{
					if (pred->brTrue.get() == block)
						pred->brTrue = tmp;
					if (pred->brFalse.get() == block)
						pred->brFalse = tmp;
				}
			}
			if (block->brTrue && block->brFalse)
			{
				assert(block->phi.empty());
				auto spliceEnd = std::prev(block->ins.end());
				if (block->ins.size() >= 2
					&& block->ins.back().oper == Br
					&& std::prev(block->ins.end(), 2)->dst.val == block->ins.back().src1.val
					&& std::prev(block->ins.end(), 2)->dst.ver == block->ins.back().src1.ver)
				{
					--spliceEnd;
				}
				if (spliceEnd != block->ins.begin())
				{
					auto preds = block->preds;
					std::shared_ptr<Block> tmp = Block::construct();
					tmp->ins.splice(tmp->ins.end(), block->ins, block->ins.begin(), spliceEnd);
					tmp->ins.push_back(IRJump());
					tmp->brTrue = block->self.lock();
					for (Block *pred : preds)
					{
						if (pred->brTrue.get() == block)
							pred->brTrue = tmp;
						if (pred->brFalse.get() == block)
							pred->brFalse = tmp;
					}
					if (block == inBlock.get())
						inBlock = tmp;
				}
			}
		}
	}

	void Block::redirectPhiSrc(Block *from, Block *to)
	{
		for (auto &kv : phi)
			for (auto &src : kv.second.srcs)
				if (src.second.lock().get() == from)
					src.second = to->self;
	}

	void Function::mergeBlocks()
	{
		std::set<Block *> blocks;
		inBlock->traverse([&blocks](Block *block) -> bool
		{
			blocks.insert(block);
			return true;
		});

		for (auto iter = blocks.begin(); iter != blocks.end(); )
		{
			std::shared_ptr<Block> block = (*iter)->self.lock();
			if (block == outBlock)
			{
				++iter;
				continue;
			}
			if (block->preds.size() == 1)
			{
				Block *pred = block->preds.front();
				if (pred->brTrue.get() == block.get() && !pred->brFalse)
				{
					pred->ins.pop_back();
					pred->ins.splice(pred->ins.end(), block->ins);

					if(block->brTrue)
						block->brTrue->redirectPhiSrc(block.get(), pred);
					if (block->brFalse)
						block->brFalse->redirectPhiSrc(block.get(), pred);

					pred->brTrue = block->brTrue;
					pred->brFalse = block->brFalse;
					block->brTrue.reset();
					block->brFalse.reset();
					iter = blocks.erase(iter);
					continue;
				}
			}
			if (block->ins.size() == 1 && block->ins.back().oper == Jump)
			{
				std::map<Operand, Block::PhiIns> phi;
				for (auto &kv : block->phi)
					phi[kv.second.dst] = kv.second;

				Block *next = block->brTrue.get();
				for (auto &kv : next->phi) //TODO: check if it is correct
				{
					decltype(kv.second.srcs) newSrc;
					for (auto iterSrc = kv.second.srcs.begin(); iterSrc != kv.second.srcs.end(); ++iterSrc)
					{
						if (iterSrc->second.lock() == block)
						{
							if (!phi.count(iterSrc->first))
							{
								for (Block *pred : block->preds)
									newSrc.push_back(std::make_pair(EmptyOperand(), pred->self));
							}
							else
							{
								for (auto &src : phi[iterSrc->first].srcs)
									newSrc.push_back(src);
							}
						}
						else
							newSrc.push_back(*iterSrc);
					}
					kv.second.srcs = newSrc;
				}

				for (Block *pred : std::list<Block *>(block->preds))
				{
					if (pred->brTrue.get() == block.get())
						pred->brTrue = next->self.lock();
					if (pred->brFalse.get() == block.get())
						pred->brFalse = next->self.lock();
				}
				block->brTrue.reset();
				iter = blocks.erase(iter);
				continue;
			}
			++iter;
		}
	}
}