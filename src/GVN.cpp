#include "common_headers.h"
#include "GVN.h"
#include <boost/uuid/sha1.hpp>

namespace MxIR
{
	void GVN::ValueHash::process_bytes(const void *buffer, size_t length)
	{
		const char *begin = static_cast<const char *>(buffer);
		const char *end = static_cast<const char *>(buffer)+length;
		for (; begin != end; ++begin)
			this->buffer.push_back(*begin);
	}

	void GVN::ValueHash::cacluateHash()
	{
		boost::uuids::detail::sha1 sha1;
		for (char c : buffer)
			sha1.process_byte(c);
		sha1.get_digest(hash);
		IFNOT_DEBUG(buffer.clear());
	}

	void GVN::ValueHash::printHash(std::ostream &out)
	{
		auto oldflag = out.flags();

		out << std::setfill('0');
		for (size_t i = 0; i < 5; i++)
			out << std::setw(8) << std::hex << hash[i];

		out.flags(oldflag);
	}

	bool GVN::ValueHash::operator<(const ValueHash &rhs) const
	{
		for (size_t i = 0; i < 5; i++)
		{
			if (hash[i] < rhs.hash[i])
				return true;
			else if (hash[i] > rhs.hash[i])
				return false;
		}
		return false;
	}

	bool GVN::ValueHash::operator==(const ValueHash &rhs) const
	{
		for (size_t i = 0; i < 5; i++)
			if (hash[i] != rhs.hash[i])
				return false;
		return true;
	}

	GVN::ValueImm::ValueImm(Operand operand) : ValueNode(Imm)
	{
		assert(operand.isImm());

		std::uint8_t imm8;
		std::uint16_t imm16;
		std::uint32_t imm32;
		std::uint64_t imm64;
		switch (operand.type)
		{
		case Operand::imm8:
			val = imm8 = std::uint8_t(operand.val);
			length = 1;
			hash.process(char(length));
			hash.process(imm8);
			break;
		case Operand::imm16:
			val = imm16 = std::uint16_t(operand.val);
			length = 2;
			hash.process(char(2));
			hash.process(imm16);
			break;
		case Operand::imm32:
			val = imm32 = std::uint32_t(operand.val);
			length = 4;
			hash.process(char(4));
			hash.process(imm32);
			break;
		case Operand::imm64:
			val = imm64 = operand.val;
			length = 8;
			hash.process(char(8));
			hash.process(imm64);
			break;
		default:
			assert(false);
		}
		hash.cacluateHash();
	}

	GVN::ValueConst::ValueConst(Operand operand) : ValueNode(Const, operand.size()), type(operand.type), val(operand.val)
	{
		assert(operand.isConst());

		hash.process(operand.type);
		hash.process(operand.val);
		hash.cacluateHash();
	}

	GVN::ValueVar::ValueVar(Operand operand) : ValueNode(Var, operand.size()), varID(operand.val), ver(operand.ver)
	{
		assert(operand.isReg());

		hash.process(varID);
		hash.process(ver);
		hash.cacluateHash();
	}

	GVN::ValueCommAssoc::ValueCommAssoc(Operator oper, size_t length, 
		const std::vector<std::shared_ptr<ValueNode>> &varValue, ValueImm immValue) : 
		ValueNode(OperCommAssoc, length), oper(oper), varValue(varValue), immValue(immValue)
	{
		std::sort(this->varValue.begin(), this->varValue.end());

		hash.process(oper);
		for (auto &node : this->varValue)
			hash.process(node->hash.getHash());
		hash.process(immValue.hash.getHash());
		hash.cacluateHash();
	}

	GVN::ValueBinary::ValueBinary(Operator oper, size_t length, std::shared_ptr<ValueNode> valueL, std::shared_ptr<ValueNode> valueR) :
		ValueNode(OperBinary, length), oper(oper), valueL(valueL), valueR(valueR)
	{
		if (oper == Seq && !(this->valueL < this->valueR))
			std::swap(this->valueL, this->valueR);
		hash.process(oper);
		hash.process(this->valueL->hash.getHash());
		hash.process(this->valueR->hash.getHash());
		hash.cacluateHash();
	}

	GVN::ValueUnary::ValueUnary(Operator oper, size_t length, std::shared_ptr<ValueNode> operand) :
		ValueNode(OperUnary, length), oper(oper), operand(operand)
	{
		hash.process(oper);
		hash.process(operand->hash.getHash());
		hash.cacluateHash();
	}

	GVN::ValueFuncCall::ValueFuncCall(size_t length, size_t funcID, const std::vector<std::shared_ptr<ValueNode>> &params) :
		ValueNode(OperFuncCall, length), funcID(funcID), params(params)
	{
		hash.process(funcID);
		for (auto &node : params)
			hash.process(node->hash.getHash());
		hash.cacluateHash();
	}

	GVN::ValuePhiIf::ValuePhiIf(size_t length, std::shared_ptr<ValueNode> cond, std::shared_ptr<ValueNode> valueTrue, std::shared_ptr<ValueNode> valueFalse) :
		ValueNode(OperPhiIf, length), cond(cond), valueTrue(valueTrue), valueFalse(valueFalse)
	{
		hash.process(cond->hash.getHash());
		hash.process(valueTrue->hash.getHash());
		hash.process(valueFalse->hash.getHash());
		hash.cacluateHash();
	}

	GVN::ValuePhi::ValuePhi(size_t length, size_t blockID, const std::vector<std::shared_ptr<ValueNode>> &srcs) :
		ValueNode(OperPhi, length), blockID(blockID), srcs(srcs)
	{
		hash.process(blockID);
		for (auto &node : srcs)
			hash.process(node->hash.getHash());
		hash.cacluateHash();
	}

	std::uint64_t GVN::ValueCommAssoc::caculate(std::uint64_t val1, Operator oper, std::uint64_t val2)
	{
		switch (oper)
		{
		case Add:
			return val1 + val2;
		case Mult:
			return val1 * val2;
		case And:
			return val1 & val2;
		case Or:
			return val1 | val2;
		case Xor:
			return val1 ^ val2;
		default:
			assert(false);
		}
		return 0;
	}

	bool GVN::ValueCommAssoc::equal(ValueNode &rhs)
	{
		if (!equal_impl(*this, rhs, &ValueCommAssoc::oper))
			return false;
		ValueCommAssoc &rnode = dynamic_cast<ValueCommAssoc &>(rhs);
		if (immValue.val != rnode.immValue.val)
			return false;
		if (varValue.size() != rnode.varValue.size())
			return false;
		for (size_t i = 0; i < varValue.size(); i++)
			if (!varValue[i]->equal(*rnode.varValue[i]))
				return false;
		return true;
	}

	bool GVN::ValueFuncCall::equal(ValueNode &rhs)
	{
		if (!equal_impl(*this, rhs, &ValueFuncCall::funcID))
			return false;
		ValueFuncCall &rnode = dynamic_cast<ValueFuncCall &>(rhs);
		if (params.size() != rnode.params.size())
			return false;
		for (size_t i = 0; i < params.size(); i++)
			if (!params[i]->equal(*rnode.params[i]))
				return false;
		return true;
	}

	bool GVN::isConstExpr(const Instruction &insn)
	{
		if (insn.oper == Load || insn.oper == LoadA || insn.oper == Store || insn.oper == StoreA || insn.oper == Allocate)
			return false;
		if (insn.oper == Call && (insn.src1.type != Operand::funcID || !(program->vFuncs[insn.src1.val].attribute & ConstExpr)))
			return false;
		return true;
	}

	std::shared_ptr<GVN::ValueNode> GVN::getOperand(Operand operand)
	{
		if (operand.isReg())
		{
			assert(opNumber.count(operand));
			return opNumber[operand];
		}
		if (operand.isImm())
			return std::shared_ptr<ValueNode>(new ValueImm(operand));
		return std::shared_ptr<ValueNode>(new ValueConst(operand));
	}

	std::shared_ptr<GVN::ValueNode> GVN::reduceValue(std::shared_ptr<ValueNode> value)
	{
		if (ValueUnary *unary = dynamic_cast<ValueUnary *>(value.get()))
		{
			if (ValueUnary *unaryOperand = dynamic_cast<ValueUnary *>(unary->operand.get()))
			{
				if (unary->oper == unaryOperand->oper
					&& (unary->oper == ValueUnary::Not || unary->oper == ValueUnary::Neg || unary->oper == ValueUnary::NotBool))
				{
					return unaryOperand->operand;
				}
			}
			else if (ValueCommAssoc *unaryOperand = dynamic_cast<ValueCommAssoc *>(unary->operand.get()))
			{
				if(unary->oper == ValueUnary::Neg && unaryOperand->oper == ValueCommAssoc::Add)
				{
					std::vector<std::shared_ptr<ValueNode>> varVal;
					for (auto &oldVal : unaryOperand->varValue)
						varVal.emplace_back(reduceValue(new ValueUnary(ValueUnary::Neg, oldVal->length, oldVal)));
					std::uint64_t immVal = -std::int64_t(unaryOperand->immValue.val);

					return std::shared_ptr<ValueNode>(new ValueCommAssoc(
						ValueCommAssoc::Add, 
						unaryOperand->length, 
						varVal, 
						ValueImm(ImmSize(immVal, unaryOperand->length))));
				}
				else if (unary->oper == ValueUnary::Neg && unaryOperand->oper == ValueCommAssoc::Mult)
				{
					return std::shared_ptr<ValueNode>(new ValueCommAssoc(
						ValueCommAssoc::Mult, 
						unaryOperand->length, 
						unaryOperand->varValue, 
						ValueImm(ImmSize(-std::int64_t(unaryOperand->immValue.val), unaryOperand->immValue.length))));
				}
				else if (unary->oper == ValueUnary::Not && (unaryOperand->oper == ValueCommAssoc::And || unaryOperand->oper == ValueCommAssoc::Or))
				{
					std::vector<std::shared_ptr<ValueNode>> varVal;
					for (auto &oldVal : unaryOperand->varValue)
					{
						varVal.emplace_back(reduceValue(new ValueUnary(
							ValueUnary::Not, oldVal->length, oldVal)));
					}
					std::uint64_t immVal = ~unaryOperand->immValue.val;

					return std::shared_ptr<ValueNode>(new ValueCommAssoc(
						unaryOperand->oper == ValueCommAssoc::And ? ValueCommAssoc::Or : ValueCommAssoc::And,
						unaryOperand->length,
						varVal,
						ValueImm(ImmSize(immVal, unaryOperand->length))));
				}
				else if (unary->oper == ValueUnary::Not && unaryOperand->oper == ValueCommAssoc::Xor)
				{
					return std::shared_ptr<ValueNode>(new ValueCommAssoc(
						ValueCommAssoc::Xor,
						unaryOperand->length,
						unaryOperand->varValue,
						ValueImm(ImmSize(~unaryOperand->immValue.val, unaryOperand->immValue.length))));
				}
			}
		}
		else if (ValuePhiIf *phi = dynamic_cast<ValuePhiIf *>(value.get()))
		{
			if (phi->valueTrue->equal(*phi->valueFalse))
				return phi->valueTrue;

			if (ValueUnary *unaryCond = dynamic_cast<ValueUnary *>(phi->cond.get()))
			{
				if (unaryCond->oper == ValueUnary::NotBool)
					return reduceValue(new ValuePhiIf(phi->length, unaryCond->operand, phi->valueFalse, phi->valueTrue));
			}
		}
		else if (ValueBinary *binary = dynamic_cast<ValueBinary *>(value.get()))
		{
			if (binary->oper == ValueBinary::Seq && binary->valueL->length == binary->length)
			{
				ValueImm *imm = dynamic_cast<ValueImm *>(binary->valueR.get());
				auto val = binary->valueL;
				if (!(imm && val))
				{
					imm = dynamic_cast<ValueImm *>(binary->valueL.get());
					val = binary->valueR;
				}
				if (imm && val)
				{
					assert(val->length == binary->length);
					return reduceValue(new ValueUnary(ValueUnary::NotBool, binary->length, val));
				}
			}
		}
		return value;
	}

	void GVN::computeDomTree()
	{
		func.inBlock->traverse([this](Block *block) -> bool
		{
			blockIndex[block] = vBlocks.size();
			vBlocks.push_back(block);
			return true;
		});
		dtree = DomTree(vBlocks.size());
		postdtree = DomTree(vBlocks.size());
		for (Block *block : vBlocks)
		{
			if (block->brTrue)
			{
				dtree.link(blockIndex[block], blockIndex[block->brTrue.get()]);
				postdtree.link(blockIndex[block->brTrue.get()], blockIndex[block]);
			}
			if (block->brFalse)
			{
				dtree.link(blockIndex[block], blockIndex[block->brFalse.get()]);
				postdtree.link(blockIndex[block->brFalse.get()], blockIndex[block]);
			}
		}
		dtree.buildTree(0);
		postdtree.buildTree(blockIndex[func.outBlock.get()]);

		depth.resize(vBlocks.size());
		computeDomTreeDepth(0, 0);
	}

	void GVN::computeDomTreeDepth(size_t idx, size_t curdepth)
	{
		depth[idx] = curdepth;
		for (size_t child : dtree.getDomChildren(idx))
			computeDomTreeDepth(child, curdepth + 1);
	}

	std::shared_ptr<GVN::ValueNode> GVN::numberInstruction(Instruction insn)
	{
		if (insn.getOutputReg().empty())
			return nullptr;
		assert(insn.getOutputReg().size() == 1);

		bool ready = true;
		for (Operand *operand : insn.getInputReg())
			if (!opNumber.count(*operand))
			{
				ready = false;
				break;
			}
		assert(ready);
		if (!ready || !isConstExpr(insn) || insn.oper == TestZero)	//TODO
			return std::shared_ptr<ValueNode>(new ValueVar(insn.dst));

		if (insn.oper == Move)
			return getOperand(insn.src1);

		static const std::map<Operation, ValueBinary::Operator> mapOperBinary = {
			{Div, ValueBinary::Div}, {Mod, ValueBinary::Mod}, {Shl, ValueBinary::Shl}, 
			{Shr, ValueBinary::Shr}, {Shlu, ValueBinary::Shlu}, {Shru, ValueBinary::Shru},
		};
		if (mapOperBinary.count(insn.oper))
		{
			return reduceValue(new ValueBinary(mapOperBinary.find(insn.oper)->second, insn.dst.size(), getOperand(insn.src1), getOperand(insn.src2)));
		}

		static const std::map<Operation, ValueUnary::Operator> mapOperUnary = {
			{Not, ValueUnary::Not}, {Neg, ValueUnary::Neg}, {Sext, ValueUnary::Sext}, {Zext, ValueUnary::Zext},
		};
		if (mapOperUnary.count(insn.oper))
			return reduceValue(new ValueUnary(mapOperUnary.find(insn.oper)->second, insn.dst.size(), getOperand(insn.src1)));

		static const std::map<Operation, std::pair<ValueCommAssoc::Operator, std::uint64_t>> mapOperCA = {
			{Add,	{ValueCommAssoc::Add, 0ULL}},	{Sub, {ValueCommAssoc::Add, 0ULL}},
			{Mult,	{ValueCommAssoc::Mult, 1ULL}},	{And, {ValueCommAssoc::And, ~0ULL}},
			{Or,	{ValueCommAssoc::Or, 0ULL}},	{Xor, {ValueCommAssoc::Xor, 0ULL}},
		};
		if (mapOperCA.count(insn.oper))
		{
			ValueCommAssoc::Operator oper = mapOperCA.find(insn.oper)->second.first;
			std::uint64_t immVal = mapOperCA.find(insn.oper)->second.second;

			auto oper1 = getOperand(insn.src1), oper2 = getOperand(insn.src2);
			std::vector<std::shared_ptr<ValueNode>> varVal;
			
			for (auto *operand : { &oper1, &oper2 })
			{
				ValueCommAssoc *childCA = dynamic_cast<ValueCommAssoc *>(operand->get());
				if (childCA && childCA->oper == oper)
				{
					if (insn.oper == Sub && operand == &oper2)
					{
						for (auto &var : childCA->varValue)
							varVal.emplace_back(reduceValue(new ValueUnary(ValueUnary::Neg, var->length, var)));
						immVal -= childCA->immValue.val;
					}
					else
					{
						std::copy(childCA->varValue.begin(), childCA->varValue.end(), std::back_inserter(varVal));
						immVal = ValueCommAssoc::caculate(immVal, oper, childCA->immValue.val);
					}
				}
				else if (ValueImm *child = dynamic_cast<ValueImm *>(operand->get()))
				{
					if (insn.oper == Sub && operand == &oper2)
						immVal -= child->val;
					else
						immVal = ValueCommAssoc::caculate(immVal, oper, child->val);
				}
				else
				{
					if (insn.oper == Sub && operand == &oper2)
						varVal.push_back(reduceValue(new ValueUnary(ValueUnary::Neg, operand->get()->length, *operand)));
					else
						varVal.push_back(*operand);
				}
			}

			if (varVal.empty())
				return std::shared_ptr<ValueNode>(new ValueImm(ImmSize(immVal, insn.dst.size())));

			return std::shared_ptr<ValueNode>(new ValueCommAssoc(
				oper, insn.dst.size(), varVal, ValueImm(ImmSize(immVal, insn.dst.size()))));
		}

		static const std::map<Operation, std::tuple<ValueBinary::Operator, bool, bool>> mapCond = {	//<operator, rev flag, not flag>
			{Seq, std::make_tuple(ValueBinary::Seq, false, false)},
			{Sne, std::make_tuple(ValueBinary::Seq, false, true)},
			{Slt, std::make_tuple(ValueBinary::Slt, false, false)},
			{Sge, std::make_tuple(ValueBinary::Slt, false, true)},
			{Sgt, std::make_tuple(ValueBinary::Slt, true, false)},
			{Sle, std::make_tuple(ValueBinary::Slt, true, true)},
			{Sltu, std::make_tuple(ValueBinary::Sltu, false, false)},
			{Sgeu, std::make_tuple(ValueBinary::Sltu, false, true)},
			{Sgtu, std::make_tuple(ValueBinary::Sltu, true, false)},
			{Sleu, std::make_tuple(ValueBinary::Sltu, true, true)},
		};
		if (mapCond.count(insn.oper))
		{
			ValueBinary::Operator oper;
			bool revflag, notflag;
			std::tie(oper, revflag, notflag) = mapCond.find(insn.oper)->second;

			auto operL = getOperand(insn.src1), operR = getOperand(insn.src2);
			if (revflag)
				std::swap(operL, operR);

			std::shared_ptr<ValueNode> tmp(new ValueBinary(oper, insn.dst.size(), operL, operR));

			if (notflag)
				return reduceValue(new ValueUnary(ValueUnary::NotBool, insn.dst.size(), tmp));
			else
				return reduceValue(tmp);
		}

		if (insn.oper == Call)
		{
			std::vector<std::shared_ptr<ValueNode>> vParam;
			for (auto &op : insn.paramExt)
				vParam.push_back(getOperand(op));
			return std::shared_ptr<ValueNode>(new ValueFuncCall(insn.dst.size(), insn.src1.val, vParam));
		}

		assert(false);
		return nullptr;
	}

	bool GVN::isPostDom(size_t parent, size_t child)
	{
		if (parent == child)
			return true;
		for (size_t next : postdtree.getDomChildren(parent))
			if (isPostDom(next, child))
				return true;
		return false;
	}

	std::shared_ptr<GVN::ValueNode> GVN::numberPhiInstruction(Block::PhiIns phiins, Block *block)
	{
		bool ready = true;
		for (Operand *operand : phiins.getInputReg())
			if (!opNumber.count(*operand))
			{
				ready = false;
				break;
			}
		if(!ready)
			return std::shared_ptr<ValueNode>(new ValueVar(phiins.dst));

		if (blacklist.count(block))
		{
			std::map<Block *, Operand> src2val;
			for (auto &src : phiins.srcs)
				src2val[src.second.lock().get()] = src.first;

			std::vector<std::shared_ptr<ValueNode>> srcs;
			for (Block *pred : block->preds)
			{
				assert(src2val.count(pred));
				if (src2val[pred].type == Operand::empty)
					srcs.emplace_back(new ValueImm(ImmSize(0, phiins.dst.size())));
				else
					srcs.push_back(getOperand(src2val[pred]));
			}
			return std::shared_ptr<ValueNode>(new ValuePhi(phiins.dst.size(), blockIndex[block], srcs));
		}

		struct TState
		{
			std::shared_ptr<ValueNode> curVal;
			Block *startBlock;
			Block *curBlock;
			Block *lastBlock;
		};

		std::list<TState> state;
		for (auto &src : phiins.srcs)
		{
			TState tmp;
			if (src.first.type == Operand::empty)
				tmp.curVal.reset(new ValueImm(ImmSize(0, phiins.dst.size())));
			else
				tmp.curVal = getOperand(src.first);

			tmp.startBlock = tmp.curBlock = src.second.lock().get();
			tmp.lastBlock = block;

			state.push_back(tmp);
		}

		state.sort([this](const TState &a, const TState &b) { return depth[blockIndex[a.curBlock]] > depth[blockIndex[b.curBlock]]; });

		while (state.size() > 1)
		{
			std::map<Block *, std::list<TState>::iterator> mapPosition;
			for (auto iter = state.begin();
				iter != state.end() 
				&& (iter == state.begin() || depth[blockIndex[iter->curBlock]] == depth[blockIndex[std::prev(iter)->curBlock]]);
				++iter)
			{
				if (!mapPosition.count(iter->curBlock))
					mapPosition[iter->curBlock] = iter;
				else
				{
					auto merged = mapPosition[iter->curBlock];
					if(!isPostDom(blockIndex[iter->startBlock], blockIndex[iter->lastBlock])
						|| !isPostDom(blockIndex[merged->startBlock], blockIndex[merged->lastBlock]))
					{
						blacklist.insert(block);
						return numberPhiInstruction(phiins, block);
					}

					if (iter->lastBlock == iter->curBlock->brTrue.get() && merged->lastBlock == iter->curBlock->brFalse.get())
					{
						assert(iter->curBlock->ins.back().oper == Br);
						iter->curVal = reduceValue(new ValuePhiIf(
							phiins.dst.size(),
							getOperand(iter->curBlock->ins.back().src1),
							iter->curVal, merged->curVal));
						iter->startBlock = iter->curBlock;
					}
					else if (iter->lastBlock == iter->curBlock->brFalse.get() && merged->lastBlock == iter->curBlock->brTrue.get())
					{
						assert(iter->curBlock->ins.back().oper == Br);
						iter->curVal = reduceValue(new ValuePhiIf(
							phiins.dst.size(),
							getOperand(iter->curBlock->ins.back().src1),
							merged->curVal, iter->curVal));
						iter->startBlock = iter->curBlock;
					}
					else
					{
						assert(false);
						blacklist.insert(block);
						return numberPhiInstruction(phiins, block);
					}

					state.erase(merged);
				}
			}

			if (state.size() <= 1)
				break;

			for (auto iter = state.begin(); iter != state.end(); ++iter)
			{
				bool flag = false;
				if (std::next(iter) != state.end() && depth[blockIndex[iter->curBlock]] != depth[blockIndex[std::next(iter)->curBlock]])
					flag = true;
				iter->lastBlock = iter->curBlock;
				iter->curBlock = vBlocks[dtree.getIdom(blockIndex[iter->curBlock])];
				if (flag)
					break;
			}
		}

		return state.front().curVal;
	}

	void GVN::computeVarGroup()
	{
		std::map<ValueHash, size_t> hash2group;
		for (auto &kv : opNumber)
		{
			ValueHash &hash = kv.second->hash;
			if (hash2group.count(hash))
			{
				if (opNumber[*varGroups[hash2group[hash]].begin()]->equal(*kv.second))
				{
					groupID[kv.first] = hash2group[hash];
					varGroups[groupID[kv.first]].insert(kv.first);
					continue;
				}
				else
				{
					std::cerr << "WARNING: Found a collision of hash: ";
					hash.printHash(std::cerr);
					std::cerr << std::endl;
				}
			}
			else
				hash2group[hash] = varGroups.size();

			groupID[kv.first] = varGroups.size();
			varGroups.emplace_back(std::set<Operand>{ kv.first });
		}
	}

	void GVN::renameVar(size_t idx, std::map<size_t, Operand> &avaliableGroup)
	{
		Block *block = vBlocks[idx];
		std::set<size_t> definedInBlock;

		for (auto &ins : block->instructions())
		{
			for (Operand *operand : ins.getInputReg())
			{
				if (!avaliableGroup.count(groupID[*operand]))
					continue;

				Operand alter = avaliableGroup[groupID[*operand]];
				if (ValueImm *imm = dynamic_cast<ValueImm *>(opNumber[alter].get()))
				{
					*operand = ImmSize(imm->val, operand->size());
				}
				else
				{
					operand->val = alter.val;
					operand->ver = alter.ver;
				}
			}
			for (Operand *operand : ins.getOutputReg())
			{
				if (!avaliableGroup.count(groupID[*operand]))
				{
					avaliableGroup[groupID[*operand]] = *operand;
					definedInBlock.insert(groupID[*operand]);
				}
			}
		}

		for (size_t child : dtree.getDomChildren(idx))
			renameVar(child, avaliableGroup);

		for (size_t group : definedInBlock)
			avaliableGroup.erase(group);
	}

	void GVN::renameVar()
	{
		std::map<size_t, Operand> avaliableGroup;
		for (auto &param : func.params)
			avaliableGroup[groupID[param]] = param;

		renameVar(0, avaliableGroup);
	}

	void GVN::work()
	{
		computeDomTree();
		for (auto &param : func.params)
		{
			opNumber[param] = std::shared_ptr<ValueNode>(new ValueVar(param));
		}
		func.inBlock->traverse_rev_postorder([this](Block *block) -> bool
		{
			for (auto &kv : block->phi)
				opNumber[kv.second.dst] = numberPhiInstruction(kv.second, block);
			for (auto &ins : block->ins)
			{
				auto output = ins.getOutputReg();
				if (!output.empty() && output[0]->isReg())
				{
					opNumber[*output[0]] = numberInstruction(ins);
					assert(opNumber[*output[0]]);
				}
			}
			return true;
		});
		computeVarGroup();

		/*for (auto &kv : opNumber)
		{
			std::cerr << kv.first.val << "_" << kv.first.ver << ": " << groupID[kv.first] << "  ";
			kv.second->hash.printHash(std::cerr);
			std::cerr << std::endl;
		}*/
		renameVar();
	}
}