#ifndef MX_COMPILER_GVN_H
#define MX_COMPILER_GVN_H

#include "common.h"
#include "IR.h"
#include "MxProgram.h"
#include "utils/DomTree.h"

namespace MxIR
{
	class GVN
	{
	public:
		GVN(Function &func) : func(func), program(MxProgram::getDefault()) {}
		void work();

	protected:
		enum NodeType : std::uint8_t
		{
			Imm, Var, Const, OperCommAssoc, OperBinary, OperUnary, OperFuncCall, OperPhiIf, OperPhi
		};
		class ValueHash
		{
		public:
			template<typename T>
			void process(const T &value)
			{
				process_bytes(&value, sizeof(value));
			}
			void process_bytes(const void *buffer, size_t length);
			void cacluateHash();

			bool operator<(const ValueHash &rhs) const;
			bool operator==(const ValueHash &rhs) const;
			bool operator!=(const ValueHash &rhs) const { return !(*this == rhs); }

			void printHash(std::ostream &out);
			const std::uint32_t (&getHash() const)[5] { return hash; }
			

		protected:
			std::uint32_t hash[5];
			std::vector<char> buffer;
		};
		struct ValueNode
		{
			const NodeType nodeType;
			ValueHash hash;
			size_t length;

			ValueNode(NodeType nodeType) : nodeType(nodeType)
			{
				hash.process(char(nodeType));
			}
			ValueNode(NodeType nodeType, size_t length) : nodeType(nodeType), length(length) 
			{
				hash.process(char(nodeType));
				hash.process(char(length));
			}
			virtual ~ValueNode() {}

			bool operator<(const ValueNode &rhs) const { return hash < rhs.hash; }
			virtual bool equal(ValueNode &rhs) = 0;

		protected:
			template<class T, class ...Tparam>
			static bool equal_impl(T &lhs, ValueNode &rhs, Tparam ...param)
			{
				if (lhs.nodeType != rhs.nodeType || lhs.length != rhs.length || lhs.hash != rhs.hash)
					return false;
				T &node = dynamic_cast<T &>(rhs);
				return equal_recursive(lhs, node, param...);
			}

		private:
			template<class T>
			static bool equal_recursive(T &lhs, T &rhs) { return true; }
			template<class T, class Tparam, class ...Tother>
			static bool equal_recursive(T &lhs, T &rhs, Tparam param, Tother ...other)
			{
				if (lhs.*param != rhs.*param)
					return false;
				return equal_recursive(lhs, rhs, other...);
			}
			template<class T, class ...Tother>
			static bool equal_recursive(T &lhs, T &rhs, std::shared_ptr<ValueNode> T::*param, Tother ...other)
			{
				if (!((lhs.*param).get()->equal(*(rhs.*param))))
					return false;
				return equal_recursive(lhs, rhs, other...);
			}
			template<class T, class ...Tother>
			static bool equal_recursive(T &lhs, T &rhs, std::vector<std::shared_ptr<ValueNode>> T::*param, Tother ...other)
			{
				if ((lhs.*param).size() != (rhs.*param).size())
					return false;
				for (size_t i = 0; i < (lhs.*param).size(); i++)
					if (!(lhs.*param)[i]->equal(*(rhs.*param)[i]))
						return false;
				return equal_recursive(lhs, rhs, other...);
			}
		};
		struct ValueImm : public ValueNode
		{
			std::uint64_t val;

			ValueImm(Operand operand);
			virtual bool equal(ValueNode &rhs) override { return equal_impl(*this, rhs, &ValueImm::val); }
		};
		struct ValueConst : public ValueNode
		{
			Operand::OperandType type;
			std::uint64_t val;

			ValueConst(Operand operand);
			virtual bool equal(ValueNode &rhs) override { return equal_impl(*this, rhs, &ValueConst::type, &ValueConst::val); }
		};
		struct ValueVar : public ValueNode
		{
			size_t varID, ver;

			ValueVar(Operand operand);
			virtual bool equal(ValueNode &rhs) override { return equal_impl(*this, rhs, &ValueVar::varID, &ValueVar::ver); }
		};
		struct ValueCommAssoc : public ValueNode
		{
			enum Operator { Add, Mult, And, Or, Xor };
			Operator oper;
			std::vector<std::shared_ptr<ValueNode>> varValue;
			ValueImm immValue;

			ValueCommAssoc(Operator oper, size_t length, const std::vector<std::shared_ptr<ValueNode>> &varValue, ValueImm immValue);
			virtual bool equal(ValueNode &rhs) override;
			static std::uint64_t calculate(std::uint64_t val1, Operator oper, std::uint64_t val2);
		};
		struct ValueBinary : public ValueNode
		{
			enum Operator { Div, Mod, Shl, Shr, Shlu, Shru, Seq, Slt, Sltu };
			Operator oper;
			std::shared_ptr<ValueNode> valueL, valueR;

			ValueBinary(Operator oper, size_t length, std::shared_ptr<ValueNode> valueL, std::shared_ptr<ValueNode> valueR);
			virtual bool equal(ValueNode &rhs) override { return equal_impl(*this, rhs, &ValueBinary::oper, &ValueBinary::valueL, &ValueBinary::valueR); }

			static std::uint64_t calculate(std::uint64_t val1, size_t length1, Operator oper, std::uint64_t val2, size_t length2, size_t lengthResult);
		};
		struct ValueUnary : public ValueNode
		{
			enum Operator { Not, Neg, Sext, Zext, NotBool };
			Operator oper;
			std::shared_ptr<ValueNode> operand;

			ValueUnary(Operator oper, size_t length, std::shared_ptr<ValueNode> operand);
			virtual bool equal(ValueNode &rhs) override { return equal_impl(*this, rhs, &ValueUnary::oper, &ValueUnary::operand); }

			static std::uint64_t calculate(std::uint64_t val, size_t length, Operator oper, size_t lengthResult);
		};
		struct ValueFuncCall : public ValueNode
		{
			size_t funcID;
			std::vector<std::shared_ptr<ValueNode>> params;

			ValueFuncCall(size_t length, size_t funcID, const std::vector<std::shared_ptr<ValueNode>> &params);
			virtual bool equal(ValueNode &rhs) override;
		};
		struct ValuePhiIf : public ValueNode
		{
			std::shared_ptr<ValueNode> cond;
			std::shared_ptr<ValueNode> valueTrue, valueFalse;
			ValuePhiIf(size_t length, std::shared_ptr<ValueNode> cond, std::shared_ptr<ValueNode> valueTrue, std::shared_ptr<ValueNode> valueFalse);
			virtual bool equal(ValueNode &rhs) override { return equal_impl(*this, rhs, &ValuePhiIf::cond, &ValuePhiIf::valueTrue, &ValuePhiIf::valueFalse); }
		};
		struct ValuePhi : public ValueNode
		{
			size_t blockID;
			std::vector<std::shared_ptr<ValueNode>> srcs;
			ValuePhi(size_t length, size_t blockID, const std::vector<std::shared_ptr<ValueNode>> &srcs);
			virtual bool equal(ValueNode &rhs) override { return equal_impl(*this, rhs, &ValuePhi::blockID, &ValuePhi::srcs); }
		};

	protected:
		bool isConstExpr(const Instruction &insn);
		std::shared_ptr<ValueNode> getOperand(Operand operand);

		static std::shared_ptr<ValueNode> reduceValue(std::shared_ptr<ValueNode> value);
		static std::shared_ptr<ValueNode> reduceValue(ValueNode *value) { return reduceValue(std::shared_ptr<ValueNode>(value)); }

		void computeDomTree();
		void computeDomTreeDepth(size_t idx, size_t curdepth);
		bool isPostDom(size_t parent, size_t child);
		std::shared_ptr<ValueNode> numberInstruction(Instruction insn);
		std::shared_ptr<ValueNode> numberPhiInstruction(Block::PhiIns phiins, Block *block);
		void computeVarGroup();

		void renameVar(size_t idx, std::map<size_t, Operand> &avaliableGroup);
		void renameVar();

	protected:
		Function &func;
		MxProgram *program;

		DomTree dtree, postdtree;
		std::vector<size_t> depth;
		
		std::set<Block *> blacklist;
		std::map<Block *, size_t> blockIndex;
		std::vector<Block *> vBlocks;

		std::map<Operand, std::shared_ptr<ValueNode>> opNumber;

		std::vector<std::set<Operand>> varGroups;
		std::map<Operand, size_t> groupID;
	};
}


#endif