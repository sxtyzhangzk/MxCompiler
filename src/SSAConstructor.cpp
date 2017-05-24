#include "common_headers.h"
#include "SSAConstructor.h"
#include "utils/ElementAdapter.h"

namespace MxIR
{
	void SSAConstructor::constructSSA()
	{
		preprocess();

		DomTree domTree = getDomTree(false);

		for (auto &kv : vars)
		{
			size_t varID = kv.first;
			const globalVar &var = kv.second;

			std::queue<size_t> workList;	//pair of { block id, var def }
			for (auto &i : var.def)
				workList.push(i);

			while (!workList.empty())
			{
				auto cur = workList.front();
				workList.pop();
				for (size_t frontier : domTree.getDomFrontier(cur))
				{
					if (!blocks[frontier]->phi.count(varID))
					{
						blocks[frontier]->phi.insert({ varID, Block::PhiIns{ var.operand } });
						if (!var.def.count(frontier))
							workList.push(frontier);
					}
				}
			}
		}

		renameVar();
	}

	void SSAConstructor::constructSSIFull()
	{
		preprocess();

		DomTree domTree = getDomTree(false);
		DomTree postDomTree = getDomTree(true);

		for (auto &kv : vars)
		{
			size_t varID = kv.first;
			const globalVar &var = kv.second;

			std::queue<size_t> workListPhi;		//blockid, operand
			std::queue<size_t> workListSigma;	//blockid
			for (size_t i : var.def)
				workListPhi.push(i);
			for (size_t i : var.uses)
				workListSigma.push(i);

			while (!workListPhi.empty() && !workListSigma.empty())
			{
				while (!workListPhi.empty())
				{
					size_t curBlock = workListPhi.front();
					workListPhi.pop();
					for (size_t frontier : domTree.getDomFrontier(curBlock))
					{
						if (!blocks[frontier]->phi.count(varID))
						{
							blocks[frontier]->phi.insert({ varID, Block::PhiIns(var.operand) });
							if (!var.def.count(frontier) && !blocks[frontier]->sigma.count(varID))
								workListPhi.push(frontier);
							if (!var.uses.count(frontier) && !blocks[frontier]->sigma.count(varID))
								workListSigma.push(frontier);
						}
					}
				}
				while (!workListSigma.empty())
				{
					size_t curBlock = workListSigma.front();
					workListSigma.pop();
					for (size_t frontier : postDomTree.getDomFrontier(curBlock))
					{
						if (!blocks[frontier]->sigma.count(varID))
						{
							blocks[frontier]->sigma.insert({ varID, Block::SigmaIns(var.operand) });
							if (!var.def.count(frontier) && !blocks[frontier]->phi.count(varID))
								workListPhi.push(frontier);
							if (!var.uses.count(frontier) && !blocks[frontier]->phi.count(varID))
								workListSigma.push(frontier);
						}
					}
				}
			}
		}
	}

	void SSAConstructor::preprocess()
	{
		maxVarID = 0;
		for (auto &param : func.params)
			maxVarID = std::max(maxVarID, param.val);
		func.inBlock->traverse([this](Block *block) -> bool
		{
			size_t myID = blocks.size();
			mapBlock.insert({ block, myID });
			blocks.push_back(block);

			std::set<size_t> killed;
			for (auto &ins : block->ins)
			{
				auto inputReg = ins.getInputReg();
				for (Operand *in : inputReg)
				{
					maxVarID = std::max(maxVarID, in->val);
					if (!killed.count(in->val))
						vars.insert({ in->val, globalVar{} });
				}

				auto outputReg = ins.getOutputReg();
				for (Operand *out : outputReg)
				{
					maxVarID = std::max(maxVarID, out->val);
					killed.insert(out->val);
				}
			}
			return true;
		});
		func.inBlock->traverse([this](Block *block) -> bool
		{
			size_t myID = mapBlock[block];

			std::set<size_t> killed;
			for (auto &ins : block->ins)
			{
				auto inputReg = ins.getInputReg();
				for (Operand *in : inputReg)
				{
					if (!vars.count(in->val) || killed.count(in->val))
						continue;
					vars[in->val].uses.insert(myID);
				}

				auto outputReg = ins.getOutputReg();
				for (Operand *out : outputReg)
				{
					if (!vars.count(out->val))
						continue;
					if (vars[out->val].operand.type == Operand::empty)
						vars[out->val].operand = *out;
					else
					{
						assert(vars[out->val].operand.type == out->type);
					}
					vars[out->val].def.insert(myID);
				}
			}
			return true;
		});
	}

	DomTree SSAConstructor::getDomTree(bool postDom)
	{
		DomTree domTree(blocks.size());
		func.inBlock->traverse([&domTree, postDom, this](Block *block) -> bool
		{
			size_t myID = mapBlock.find(block)->second;
			if (block->brTrue)
				postDom ? domTree.link(mapBlock.find(block->brTrue.get())->second, myID)
						: domTree.link(myID, mapBlock.find(block->brTrue.get())->second);
			if (block->brFalse)
				postDom ? domTree.link(mapBlock.find(block->brFalse.get())->second, myID)
						: domTree.link(myID, mapBlock.find(block->brFalse.get())->second);
			return true;
		});
		domTree.buildTree(mapBlock.find(func.inBlock.get())->second);
		return domTree;
	}

	void SSAConstructor::renameVar()
	{
		std::set<Block *> visited;
		std::vector<size_t> varCount(maxVarID + 1);
		std::vector<std::stack<Operand>> varCurVersion(maxVarID + 1);

		for (auto &param : func.params)
		{
			param.ver = ++varCount[param.val];
			varCurVersion[param.val].push(param);
		}
		for (auto &var : varCurVersion)
		{
			if (var.empty())
				var.push(EmptyOperand());
		}

		renameVar(func.inBlock.get(), visited, varCount, varCurVersion);
	}

	void SSAConstructor::renameVar(Block *block, std::set<Block *> &visited, std::vector<size_t> &varCount, std::vector<std::stack<Operand>> &varCurVersion)
	{
		visited.insert(block);
		std::set<size_t> killed;
		for (auto &phi : block->phi)
		{
			assert(phi.first == phi.second.dst.val);
			phi.second.dst.ver = ++varCount[phi.second.dst.val];
			killed.insert(phi.second.dst.val);
			varCurVersion[phi.second.dst.val].push(phi.second.dst);
		}

		for (auto &ins : block->ins)
		{
			auto inputReg = ins.getInputReg();
			for (auto &in : inputReg)
			{
				//assert(!varCurVersion[in->val].empty());
				assert(varCurVersion[in->val].top().val == in->val);
				*in = varCurVersion[in->val].top();
			}

			for (Operand *out : ins.getOutputReg())
			{
				out->ver = ++varCount[out->val];

				if (!killed.count(out->val))
					killed.insert(out->val);
				else
					varCurVersion[out->val].pop();
				varCurVersion[out->val].push(*out);
			}
		}

		if (block->brTrue && block->brTrue == block->brFalse)
		{
			assert(!block->ins.empty());
			assert(block->ins.back().oper == Br);
			block->ins.back() = IRJump();
			block->brFalse.reset();
		}

		auto visitChild = [&block, &varCount, &varCurVersion, &visited](Block::block_ptr &child, Operand Block::SigmaIns::*sigmaDst)
		{
			for (auto &sigma : block->sigma)
			{
				Operand var = sigma.second.src;
				var.ver = ++varCount[var.val];
				varCurVersion[var.val].push(var);
				sigma.second.*sigmaDst = var;
			}
			for (auto &phi : child->phi)
			{
				assert(phi.first == phi.second.dst.val);
				//assert(!varCurVersion[phi.second.dst.val].empty());
				phi.second.srcs.push_back({ varCurVersion[phi.second.dst.val].top(), block->self });
			}
			if (!visited.count(child.get()))
				renameVar(child.get(), visited, varCount, varCurVersion);
			for (auto &sigma : block->sigma)
				varCurVersion[sigma.second.src.val].pop();
		};
		if (block->brTrue)
			visitChild(block->brTrue, &Block::SigmaIns::dstTrue);
		if (block->brFalse)
			visitChild(block->brFalse, &Block::SigmaIns::dstFalse);

		for (size_t varid : killed)
			varCurVersion[varid].pop();
	}

	void SSAConstructor::constructSSA(MxProgram *program)
	{
		for (auto &func : program->vFuncs)
		{
			SSAConstructor ssa(func.content);
			ssa.constructSSA();
		}
	}

	std::map<Operand, SSAConstructor::varDefUse> SSAConstructor::calculateDefUse()
	{
		std::map<Operand, varDefUse> ret;
		func.inBlock->traverse([&ret](Block *block) -> bool
		{
			for (InstructionBase &ins : block->instructions())
			{
				for (Operand *operand : ins.getInputReg())
					ret[*operand].uses.insert(std::make_pair(&ins, operand));
				for (Operand *operand : ins.getOutputReg())
				{
					assert(!ret[*operand].def.first);
					ret[*operand].def = { &ins, operand };
				}
			}
			return true;
		});
		return ret;
	}
}