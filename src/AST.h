#ifndef MX_COMPILER_AST_H
#define MX_COMPILER_AST_H

#include "common.h"
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

namespace MxAST
{
	class ASTNode;
	class ASTBlock;
	class ASTRoot;
	class ASTDeclClass;
	class ASTDeclVar;
	class ASTDeclFunc;
	class ASTExpr;
	class ASTExprImm;
	class ASTExprVar;
	class ASTExprUnary;
	class ASTExprBinary;
	class ASTExprAssignment;
	class ASTExprNew;
	class ASTExprSubscriptAccess;
	class ASTExprMemberAccess;
	class ASTExprFuncCall;
	class ASTStatement;
	class ASTStatementReturn;
	class ASTStatementBreak;
	class ASTStatementContinue;
	class ASTStatementIf;
	class ASTStatementWhile;
	class ASTStatementFor;
	class ASTStatementBlock;
	class ASTStatementExpr;
	class ASTDeclVarLocal;
	class ASTDeclVarGlobal;

	class ASTVisitor
	{
	public:
		//WARNING: Be careful when using visit(ASTNode *node)
		virtual void visit(ASTNode *node);
		virtual void visit(ASTBlock *block);
		virtual void visit(ASTRoot *root);
		virtual void visit(ASTDeclClass *declClass);
		virtual void visit(ASTDeclVar *declVar);
		virtual void visit(ASTDeclFunc *declFunc);
		virtual void visit(ASTExpr *expr);
		virtual void visit(ASTExprImm *imm);
		virtual void visit(ASTExprVar *var);
		virtual void visit(ASTExprUnary *unary);
		virtual void visit(ASTExprBinary *binary);
		virtual void visit(ASTExprAssignment *assign);
		virtual void visit(ASTExprNew *exprNew);
		virtual void visit(ASTExprSubscriptAccess *exprSub);
		virtual void visit(ASTExprMemberAccess *expr);
		virtual void visit(ASTExprFuncCall *expr);
		virtual void visit(ASTStatement *stat);
		virtual void visit(ASTStatementReturn *stat);
		virtual void visit(ASTStatementBreak *stat);
		virtual void visit(ASTStatementContinue *stat);
		virtual void visit(ASTStatementIf *stat);
		virtual void visit(ASTStatementWhile *stat);
		virtual void visit(ASTStatementFor *stat);
		virtual void visit(ASTStatementBlock *block);
		virtual void visit(ASTStatementExpr *stat);
		virtual void visit(ASTDeclVarLocal *declVar);
		virtual void visit(ASTDeclVarGlobal *declVar);

	private:
		template<typename Base1, typename Base2, typename T>
		void defaultVisit(T *node)
		{
			visit(static_cast<Base1 *>(node));
			visit(static_cast<Base2 *>(node));
		}
	};

	class ASTListener
	{
	public:
		//if return value != parameter, it will replace the node with return value
		//if return value == nullptr, it will delete the node
		//if return value == -1, it will skip the visit of this node
		//WARNING: Be careful when using multiple inherited class
		virtual ASTNode * enter(ASTBlock *block);
		virtual ASTNode * enter(ASTDeclClass *declClass);
		virtual ASTNode * enter(ASTDeclVar *declVar);
		virtual ASTNode * enter(ASTDeclFunc *declFunc);
		virtual ASTNode * enter(ASTExpr *expr);
		virtual ASTNode * enter(ASTExprImm *expr);
		virtual ASTNode * enter(ASTExprVar *expr);
		virtual ASTNode * enter(ASTExprUnary *expr);
		virtual ASTNode * enter(ASTExprBinary *expr);
		virtual ASTNode * enter(ASTExprAssignment *expr);
		virtual ASTNode * enter(ASTExprNew *expr);
		virtual ASTNode * enter(ASTExprSubscriptAccess *expr);
		virtual ASTNode * enter(ASTExprMemberAccess *expr);
		virtual ASTNode * enter(ASTExprFuncCall *expr);
		virtual ASTNode * enter(ASTStatement *stat);
		virtual ASTNode * enter(ASTStatementReturn *stat);
		virtual ASTNode * enter(ASTStatementBreak *stat);
		virtual ASTNode * enter(ASTStatementContinue *stat);
		virtual ASTNode * enter(ASTStatementIf *stat);
		virtual ASTNode * enter(ASTStatementWhile *stat);
		virtual ASTNode * enter(ASTStatementFor *stat);
		virtual ASTNode * enter(ASTStatementBlock *stat);
		virtual ASTNode * enter(ASTStatementExpr *stat);
		virtual ASTNode * enter(ASTDeclVarLocal *declVar);
		virtual ASTNode * enter(ASTDeclVarGlobal *declVar);

		virtual ASTNode * leave(ASTBlock *block);
		virtual ASTNode * leave(ASTDeclClass *declClass);
		virtual ASTNode * leave(ASTDeclVar *declVar);
		virtual ASTNode * leave(ASTDeclFunc *declFunc);
		virtual ASTNode * leave(ASTExpr *expr);
		virtual ASTNode * leave(ASTExprImm *expr);
		virtual ASTNode * leave(ASTExprVar *expr);
		virtual ASTNode * leave(ASTExprUnary *expr);
		virtual ASTNode * leave(ASTExprBinary *expr);
		virtual ASTNode * leave(ASTExprAssignment *expr);
		virtual ASTNode * leave(ASTExprNew *expr);
		virtual ASTNode * leave(ASTExprSubscriptAccess *expr);
		virtual ASTNode * leave(ASTExprMemberAccess *expr);
		virtual ASTNode * leave(ASTExprFuncCall *expr);
		virtual ASTNode * leave(ASTStatement *stat);
		virtual ASTNode * leave(ASTStatementReturn *stat);
		virtual ASTNode * leave(ASTStatementBreak *stat);
		virtual ASTNode * leave(ASTStatementContinue *stat);
		virtual ASTNode * leave(ASTStatementIf *stat);
		virtual ASTNode * leave(ASTStatementWhile *stat);
		virtual ASTNode * leave(ASTStatementFor *stat);
		virtual ASTNode * leave(ASTStatementBlock *stat);
		virtual ASTNode * leave(ASTStatementExpr *stat);
		virtual ASTNode * leave(ASTDeclVarLocal *declVar);
		virtual ASTNode * leave(ASTDeclVarGlobal *declVar);

	private:
		template<typename Base1, typename Base2, typename T>
		ASTNode * defaultEnter(T *node)
		{
			ASTNode * ret = enter(static_cast<Base1 *>(node));
			if (ret != static_cast<Base1 *>(node))
				return ret;
			return enter(static_cast<Base2 *>(node));
		}
		template<typename Base1, typename Base2, typename T>
		ASTNode * defaultLeave(T *node)
		{
			ASTNode * ret = leave(static_cast<Base1 *>(node));
			if (ret != static_cast<Base1 *>(node))
				return ret;
			return leave(static_cast<Base2 *>(node));
		}
	};

	class ASTListenerChain
	{
	public:
		ASTListenerChain() {}
		ASTListenerChain(std::initializer_list<ASTListener *> listeners) : vListeners(listeners) {}

		template<typename T>
		ASTNode * enter(T *node) const
		{
			for (ASTListener *listener : vListeners)
			{
				ASTNode *ret = listener->enter(node);
				if (ret != static_cast<ASTNode *>(node))
					return ret;
			}
			return node;
		}
		template<typename T>
		ASTNode * leave(T *node) const
		{
			for (ASTListener *listener : vListeners)
			{
				ASTNode *ret = listener->leave(node);
				if (ret != static_cast<ASTNode *>(node))
					return ret;
			}
			return node;
		}
		void addListener(ASTListener *listener)
		{
			vListeners.push_back(listener);
		}

	protected:
		std::vector<ASTListener *> vListeners;
	};

	class ASTNode
	{
	public:
		virtual void accept(ASTVisitor *visitor) = 0;
		virtual void accept(const ASTListenerChain &listeners, std::unique_ptr<ASTNode> &container) = 0;
		virtual ~ASTNode() {}

	public:
		ssize_t tokenL, tokenR;

	protected:
		template<typename T>
		static bool listenerEnter(const ASTListenerChain &listeners, std::unique_ptr<ASTNode> &container, T *This)
		{
			assert(container.get() == static_cast<ASTNode *>(This));
			ASTNode *ret = listeners.enter(This);
			if (ret == reinterpret_cast<ASTNode *>(-1))
				return false;
			if (ret != static_cast<ASTNode *>(This))
			{
				container.reset(ret);
				return false;
			}
			return true;
		}
		template<typename T>
		static bool listenerLeave(const ASTListenerChain &listeners, std::unique_ptr<ASTNode> &container, T *This)
		{
			assert(container.get() == static_cast<ASTNode *>(This));
			ASTNode *ret = listeners.leave(This);
			if (ret != static_cast<ASTNode *>(This))
			{
				container.reset(ret);
				return false;
			}
			return true;
		}
		template<typename T>
		static void accessChildren(const ASTListenerChain &listeners, std::vector<std::unique_ptr<ASTNode>> &container)
		{
			for (auto &item : container)
			{
				if (!item)
					continue;
				assert(dynamic_cast<T *>(item.get()));
				item->accept(listeners, item);
				assert(dynamic_cast<T *>(item.get()));
			}
		}
		template<typename T>
		static void accessChildren(const ASTListenerChain &listeners, std::unique_ptr<ASTNode> &item)
		{
			if (!item)
				return;
			assert(dynamic_cast<T *>(item.get()));
			item->accept(listeners, item);
			assert(dynamic_cast<T *>(item.get()));
		}
	};

	//////////////////////////////////

	class ASTBlock : virtual public ASTNode		//Abstract
	{
	public:
		virtual void accept(const ASTListenerChain &listeners, std::unique_ptr<ASTNode> &container) override
		{
			accessChildren<ASTStatement>(listeners, stats);
		}
	public:
		std::vector<std::unique_ptr<ASTNode>> stats;
	};

	class ASTStatement : virtual public ASTNode		//Abstract
	{
	public:
		bool reachable;
	};

	class ASTExpr : virtual public ASTNode	//Abstract
	{
	public:
		MxType exprType;
		ValueType vType;
		bool sideEffect;

	protected:
		ASTExpr() : exprType(MxType{ MxType::Void }), vType(rvalue), sideEffect(true) {}
		ASTExpr(MxType exprType) : exprType(exprType), vType(rvalue), sideEffect(true) {}
		ASTExpr(MxType exprType, ValueType vType, bool sideEffect) : exprType(exprType), vType(vType), sideEffect(sideEffect) {}
	};

	class ASTRoot : public ASTNode
	{
	public:
		virtual void accept(ASTVisitor *visitor) override { visitor->visit(this); }
		virtual void accept(const ASTListenerChain &listeners, std::unique_ptr<ASTNode> &container) override
		{
			accessChildren<ASTNode>(listeners, nodes);
		}

	public:
		void recursiveAccess(const ASTListenerChain &listeners)
		{
			accessChildren<ASTNode>(listeners, nodes);
		}
		void recursiveAccess(ASTListener *listener)
		{
			recursiveAccess(ASTListenerChain({ listener }));
		}
		void recursiveAccess(std::initializer_list<ASTListener *> listeners)
		{
			recursiveAccess(ASTListenerChain(listeners));
		}

	public:
		std::vector<std::unique_ptr<ASTNode>> nodes;
	};

	class ASTDeclClass : public ASTNode
	{
	public:
		virtual void accept(ASTVisitor *visitor) override { visitor->visit(this); }
		virtual void accept(const ASTListenerChain &listeners, std::unique_ptr<ASTNode> &container) override
		{
			if (!listenerEnter(listeners, container, this))
				return;
			accessChildren<ASTDeclVar>(listeners, vMemVars);
			accessChildren<ASTDeclFunc>(listeners, vMemFuncs);
			listenerLeave(listeners, container, this);
		}
	public:
		std::vector<std::unique_ptr<ASTNode>> vMemVars;
		std::vector<std::unique_ptr<ASTNode>> vMemFuncs;
		size_t className;
		IF_DEBUG(std::string strClassName);
	};

	class ASTDeclVar : virtual public ASTNode	//Abstract
	{
	public:
		virtual void accept(const ASTListenerChain &listeners, std::unique_ptr<ASTNode> &container) override
		{
			accessChildren<ASTExpr>(listeners, initVal);
		}
	public:
		MxType varType;
		size_t varName;
		std::unique_ptr<ASTNode> initVal;
		size_t varID;
		IF_DEBUG(std::string strVarName);
	};

	class ASTDeclFunc : public ASTBlock
	{
	public:
		virtual void accept(ASTVisitor *visitor) override { visitor->visit(this); }
		virtual void accept(const ASTListenerChain &listeners, std::unique_ptr<ASTNode> &container) override
		{
			if (!listenerEnter(listeners, container, this))
				return;
			accessChildren<ASTDeclVar>(listeners, param);
			ASTBlock::accept(listeners, container);
			listenerLeave(listeners, container, this);
		}
	public:
		MxType retType;
		size_t funcName;
		size_t funcID;
		std::vector<std::unique_ptr<ASTNode>> param;
		IF_DEBUG(std::string strFuncName);
	};


	class ASTExprImm : public ASTExpr
	{
	public:
		virtual void accept(ASTVisitor *visitor) override { visitor->visit(this); }
		virtual void accept(const ASTListenerChain &listeners, std::unique_ptr<ASTNode> &container) override
		{
			listenerEnter(listeners, container, this) && listenerLeave(listeners, container, this);
		}

	public:
		//exprType Can only be { Bool, Integer, String, Object }
		//when exprType == Object, value must be null
		union
		{
			bool bvalue;
			int ivalue;
			size_t strId;
		} exprVal;
		IF_DEBUG(std::string strContent);

	public:
		ASTExprImm() : ASTExpr(MxType{ MxType::Void }, rvalue, false) {}
		ASTExprImm(bool bvalue) : ASTExpr(MxType{ MxType::Bool }, rvalue, false) { exprVal.bvalue = bvalue; }
		ASTExprImm(int ivalue) : ASTExpr(MxType{ MxType::Integer }, rvalue, false) { exprVal.ivalue = ivalue; }
		ASTExprImm(std::nullptr_t) : ASTExpr(MxType{ MxType::Object, 0, size_t(-1) }, rvalue, false) {}
	};

	class ASTExprVar : public ASTExpr
	{
	public:
		virtual void accept(ASTVisitor *visitor) override { visitor->visit(this); }
		virtual void accept(const ASTListenerChain &listeners, std::unique_ptr<ASTNode> &container) override
		{
			listenerEnter(listeners, container, this) && listenerLeave(listeners, container, this);
		}

	public:
		bool isThis, isGlobalVar;
		size_t varName;
		size_t varID;
		IF_DEBUG(std::string strVarName);
	};

	class ASTExprUnary : public ASTExpr
	{
	public:
		virtual void accept(ASTVisitor *visitor) override { visitor->visit(this); }
		virtual void accept(const ASTListenerChain &listeners, std::unique_ptr<ASTNode> &container) override
		{
			if (!listenerEnter(listeners, container, this))
				return;
			accessChildren<ASTExpr>(listeners, operand);
			listenerLeave(listeners, container, this);
		}

	public:
		enum Operator : int
		{
			IncPostfix, DecPostfix,
			Increment, Decrement,
			Positive, Negative, Not, BitNot
		};
		Operator oper;
		std::unique_ptr<ASTNode> operand;

	public:
		ASTExprUnary() {}
		ASTExprUnary(Operator oper, std::unique_ptr<ASTNode> &&operand) :
			oper(oper), operand(std::move(operand)) {}
		ASTExprUnary(Operator oper, ASTNode *operand) :
			oper(oper), operand(operand) {}

		static std::string getOperName(Operator op)
		{
			static const std::unordered_map<int, std::string> operName = {
				{IncPostfix, "++"}, {DecPostfix, "--"}, {Increment, "++"}, {Decrement, "--"},
				{Positive, "+"}, {Negative, "-"}, {Not, "!"}, {BitNot, "~"}};
			return operName.find(op)->second;
		}
		std::string getOperName()
		{
			return getOperName(oper);
		}
		static int evaluate(Operator oper, int val);
		static bool evaluate(Operator oper, bool val);
		int evaluate(int val) const { return evaluate(oper, val); }
		bool evaluate(bool val) const { return evaluate(oper, val); }
	};

	class ASTExprBinary : public ASTExpr
	{
	public:
		virtual void accept(ASTVisitor *visitor) override { visitor->visit(this); }
		virtual void accept(const ASTListenerChain &listeners, std::unique_ptr<ASTNode> &container) override
		{
			if (!listenerEnter(listeners, container, this))
				return;
			accessChildren<ASTExpr>(listeners, operandL);
			accessChildren<ASTExpr>(listeners, operandR);
			listenerLeave(listeners, container, this);
		}
	public:
		enum Operator : int
		{
			Plus, Minus, Multiple, Divide, Mod,
			ShiftLeft, ShiftRight,
			BitAnd, BitXor, BitOr,
			And, Or,
			Equal, NotEqual,
			GreaterThan, GreaterEqual, LessThan, LessEqual
		};
		Operator oper;
		std::unique_ptr<ASTNode> operandL, operandR;

	public:
		ASTExprBinary() {}
		ASTExprBinary(Operator oper, std::unique_ptr<ASTNode> &&operandL, std::unique_ptr<ASTNode> &&operandR) :
			oper(oper), operandL(std::move(operandL)), operandR(std::move(operandR)) {}
		ASTExprBinary(Operator oper, ASTNode *operandL, ASTNode *operandR) :
			oper(oper), operandL(operandL), operandR(operandR) {}

		static std::string getOperName(Operator op)
		{
			static const std::unordered_map<int, std::string> operName = {
				{Plus, "+"}, {Minus, "-"}, {Multiple, "*"}, {Divide, "/"}, {Mod, "%"},
				{ShiftLeft, "<<"}, {ShiftRight, ">>"}, {BitAnd, "&"}, {BitXor, "^"}, {BitOr, "|"},
				{And, "&&"}, {Or, "||"},
				{Equal, "=="}, {NotEqual, "!="},
				{GreaterThan, ">"}, {GreaterEqual, ">="}, {LessThan, "<"}, {LessEqual, "<="} };
			return operName.find(op)->second;
		}
		std::string getOperName()
		{
			return getOperName(oper);
		}
		static int evaluate(int valL, Operator oper, int valR);
		static bool evaluate(bool valL, Operator oper, bool valR);
		static bool stringCompare(const std::string &valL, Operator oper, const std::string &valR);
		int evaluate(int valL, int valR) const { return evaluate(valL, oper, valR); }
		bool evaluate(bool valL, bool valR) const { return evaluate(valL, oper, valR); }
		bool stringCompare(const std::string &valL, const std::string &valR) const { return stringCompare(valL, oper, valR); }
	};

	class ASTExprAssignment : public ASTExpr
	{
	public:
		virtual void accept(ASTVisitor *visitor) override { visitor->visit(this); }
		virtual void accept(const ASTListenerChain &listeners, std::unique_ptr<ASTNode> &container) override
		{
			if (!listenerEnter(listeners, container, this))
				return;
			accessChildren<ASTExpr>(listeners, operandL);
			accessChildren<ASTExpr>(listeners, operandR);
			listenerLeave(listeners, container, this);
		}
	public:
		std::unique_ptr<ASTNode> operandL, operandR;
	};

	class ASTExprNew : public ASTExpr
	{
	public:
		virtual void accept(ASTVisitor *visitor) override { visitor->visit(this); }
		virtual void accept(const ASTListenerChain &listeners, std::unique_ptr<ASTNode> &container) override
		{
			if (!listenerEnter(listeners, container, this))
				return;
			accessChildren<ASTExpr>(listeners, paramList);
			listenerLeave(listeners, container, this);
		}
	public:
		std::vector<std::unique_ptr<ASTNode>> paramList; //paramList or arraySize: new int[1][2][3][] -> {1, 2, 3, nullptr}
		bool isArray;
		size_t funcID;
	};

	class ASTExprSubscriptAccess : public ASTExpr
	{
	public:
		virtual void accept(ASTVisitor *visitor) override { visitor->visit(this); }
		virtual void accept(const ASTListenerChain &listeners, std::unique_ptr<ASTNode> &container) override
		{
			if (!listenerEnter(listeners, container, this))
				return;
			accessChildren<ASTExpr>(listeners, arr);
			accessChildren<ASTExpr>(listeners, subscript);
			listenerLeave(listeners, container, this);
		}
	public:
		std::unique_ptr<ASTNode> arr, subscript;
	};

	class ASTExprMemberAccess : public ASTExpr
	{
	public:
		virtual void accept(ASTVisitor *visitor) override { visitor->visit(this); }
		virtual void accept(const ASTListenerChain &listeners, std::unique_ptr<ASTNode> &container) override
		{
			if (!listenerEnter(listeners, container, this))
				return;
			accessChildren<ASTExpr>(listeners, object);
			listenerLeave(listeners, container, this);
		}
	public:
		std::unique_ptr<ASTNode> object;
		size_t memberName;
		size_t memberID;
		IF_DEBUG(std::string strMemberName);
	};

	class ASTExprFuncCall : public ASTExpr
	{
	public:
		virtual void accept(ASTVisitor *visitor) override { visitor->visit(this); }
		virtual void accept(const ASTListenerChain &listeners, std::unique_ptr<ASTNode> &container) override
		{
			if (!listenerEnter(listeners, container, this))
				return;
			accessChildren<ASTExpr>(listeners, func);
			accessChildren<ASTExpr>(listeners, paramList);
			listenerLeave(listeners, container, this);
		}
	public:
		size_t funcID;
		std::unique_ptr<ASTNode> func;
		std::vector<std::unique_ptr<ASTNode>> paramList;
	};

	

	class ASTStatementReturn : public ASTStatement
	{
	public:
		virtual void accept(ASTVisitor *visitor) override { visitor->visit(this); }
		virtual void accept(const ASTListenerChain &listeners, std::unique_ptr<ASTNode> &container) override
		{
			if (!listenerEnter(listeners, container, this))
				return;
			accessChildren<ASTExpr>(listeners, retExpr);
			listenerLeave(listeners, container, this);
		}
	public:
		std::unique_ptr<ASTNode> retExpr;
	};

	class ASTStatementBreak : public ASTStatement
	{
	public:
		virtual void accept(ASTVisitor *visitor) override { visitor->visit(this); }
		virtual void accept(const ASTListenerChain &listeners, std::unique_ptr<ASTNode> &container) override
		{
			listenerEnter(listeners, container, this) && listenerLeave(listeners, container, this);
		}
	};

	class ASTStatementContinue : public ASTStatement
	{
	public:
		virtual void accept(ASTVisitor *visitor) override { visitor->visit(this); }
		virtual void accept(const ASTListenerChain &listeners, std::unique_ptr<ASTNode> &container) override
		{
			listenerEnter(listeners, container, this) && listenerLeave(listeners, container, this);
		}
	};

	class ASTStatementIf : public ASTStatement, public ASTBlock
	{
	public:
		virtual void accept(ASTVisitor *visitor) override { visitor->visit(this); }
		virtual void accept(const ASTListenerChain &listeners, std::unique_ptr<ASTNode> &container) override
		{
			if (!listenerEnter(listeners, container, this))
				return;
			accessChildren<ASTExpr>(listeners, exprCond);
			accessChildren<ASTStatement>(listeners, ifStat);
			accessChildren<ASTStatement>(listeners, elseStat);
			listenerLeave(listeners, container, this);
		}
	public:
		std::unique_ptr<ASTNode> exprCond;
		std::unique_ptr<ASTNode> ifStat, elseStat;
	};

	class ASTStatementWhile : public ASTStatement, public ASTBlock
	{
	public:
		virtual void accept(ASTVisitor *visitor) override { visitor->visit(this); }
		virtual void accept(const ASTListenerChain &listeners, std::unique_ptr<ASTNode> &container) override
		{
			if (!listenerEnter(listeners, container, this))
				return;
			accessChildren<ASTExpr>(listeners, exprCond);
			ASTBlock::accept(listeners, container);
			listenerLeave(listeners, container, this);
		}
	public:
		std::unique_ptr<ASTNode> exprCond;
	};

	class ASTStatementFor : public ASTStatement, public ASTBlock
	{
	public:
		virtual void accept(ASTVisitor *visitor) override { visitor->visit(this); }
		virtual void accept(const ASTListenerChain &listeners, std::unique_ptr<ASTNode> &container) override
		{
			if (!listenerEnter(listeners, container, this))
				return;
			accessChildren<ASTDeclVarLocal>(listeners, varDecls);
			accessChildren<ASTStatementExpr>(listeners, exprIn);
			accessChildren<ASTExpr>(listeners, exprCond);
			accessChildren<ASTStatementExpr>(listeners, exprStep);
			ASTBlock::accept(listeners, container);
			listenerLeave(listeners, container, this);
		}
	public:
		std::vector<std::unique_ptr<ASTNode>> varDecls;
		std::unique_ptr<ASTNode> exprIn, exprStep;
		std::unique_ptr<ASTNode> exprCond;
	};

	class ASTStatementBlock : public ASTStatement, public ASTBlock
	{
	public:
		virtual void accept(ASTVisitor *visitor) override { visitor->visit(this); }
		virtual void accept(const ASTListenerChain &listeners, std::unique_ptr<ASTNode> &container) override
		{
			if (!listenerEnter(listeners, container, this))
				return;
			ASTBlock::accept(listeners, container);
			listenerLeave(listeners, container, this);
		}
	};

	class ASTStatementExpr : public ASTStatement
	{
	public:
		virtual void accept(ASTVisitor *visitor) override { visitor->visit(this); }
		virtual void accept(const ASTListenerChain &listeners, std::unique_ptr<ASTNode> &container) override
		{
			if (!listenerEnter(listeners, container, this))
				return;
			accessChildren<ASTExpr>(listeners, expr);
			listenerLeave(listeners, container, this);
		}
	public:
		std::unique_ptr<ASTNode> expr;
	};

	class ASTDeclVarLocal : public ASTStatement, public ASTDeclVar
	{
	public:
		virtual void accept(ASTVisitor *visitor) override { visitor->visit(this); }
		virtual void accept(const ASTListenerChain &listeners, std::unique_ptr<ASTNode> &container) override
		{
			if (!listenerEnter(listeners, container, this))
				return;
			ASTDeclVar::accept(listeners, container);
			listenerLeave(listeners, container, this);
		}
	};

	class ASTDeclVarGlobal : public ASTDeclVar
	{
	public:
		virtual void accept(ASTVisitor *visitor) override { visitor->visit(this); }
		virtual void accept(const ASTListenerChain &listeners, std::unique_ptr<ASTNode> &container) override
		{
			if (!listenerEnter(listeners, container, this))
				return;
			ASTDeclVar::accept(listeners, container);
			listenerLeave(listeners, container, this);
		}
	};

}
#endif