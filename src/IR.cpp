#include "common_headers.h"
#include "IR.h"

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
}