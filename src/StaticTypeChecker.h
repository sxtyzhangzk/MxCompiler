#ifndef MX_COMPILER_VARIABLE_CHECKER_H
#define MX_COMPILER_VARIABLE_CHECKER_H

#include "MemberTable.h"
#include "IssueCollector.h"
#include "GlobalSymbol.h"
#include "AST.h"
#include <map>
#include <stack>
#include <set>

class StaticTypeChecker : public MxAST::ASTListener
{
public:
	StaticTypeChecker(MemberTable *memTable, GlobalSymbol *symbols, IssueCollector *issues);
	bool preCheck(MxAST::ASTRoot *root);

protected:
	bool checkFunc(MxAST::ASTDeclFunc *declFunc, std::map<size_t, size_t> &mapVarId, std::vector<MemberTable::varInfo> &varTable);
	bool checkVar(MxAST::ASTDeclVar *declVar, std::map<size_t, size_t> &mapVarId, std::vector<MemberTable::varInfo> &varTable);
	bool checkType(MxType type, ssize_t tokenL, ssize_t tokenR);

	virtual MxAST::ASTNode * enter(MxAST::ASTDeclClass *declClass) override;
	virtual MxAST::ASTNode * leave(MxAST::ASTDeclClass *declClass) override;
	virtual MxAST::ASTNode * enter(MxAST::ASTDeclFunc *declFunc) override;
	virtual MxAST::ASTNode * leave(MxAST::ASTDeclFunc *declFunc) override;
	virtual MxAST::ASTNode * enter(MxAST::ASTDeclVarGlobal *declVar) override;
	virtual MxAST::ASTNode * enter(MxAST::ASTDeclVarLocal *declVar) override;
	virtual MxAST::ASTNode * enter(MxAST::ASTBlock *block) override;
	virtual MxAST::ASTNode * leave(MxAST::ASTBlock *block) override;
	virtual MxAST::ASTNode * enter(MxAST::ASTExprVar *var) override;
	virtual MxAST::ASTNode * leave(MxAST::ASTExprImm *imm) override;
	virtual MxAST::ASTNode * leave(MxAST::ASTExprUnary *expr) override;
	virtual MxAST::ASTNode * leave(MxAST::ASTExprBinary *expr) override;
	virtual MxAST::ASTNode * leave(MxAST::ASTExprAssignment *expr) override;
	virtual MxAST::ASTNode * leave(MxAST::ASTExprNew *expr) override;
	virtual MxAST::ASTNode * leave(MxAST::ASTExprSubscriptAccess *expr) override;
	virtual MxAST::ASTNode * leave(MxAST::ASTExprMemberAccess *expr) override;
	virtual MxAST::ASTNode * leave(MxAST::ASTExprFuncCall *expr) override;
	virtual MxAST::ASTNode * leave(MxAST::ASTStatementReturn *stat) override;
	virtual MxAST::ASTNode * leave(MxAST::ASTStatementBreak *stat) override;
	virtual MxAST::ASTNode * leave(MxAST::ASTStatementContinue *stat) override;
	virtual MxAST::ASTNode * leave(MxAST::ASTStatementIf *stat) override;
	virtual MxAST::ASTNode * enter(MxAST::ASTStatementWhile *stat) override;
	virtual MxAST::ASTNode * leave(MxAST::ASTStatementWhile *stat) override;
	virtual MxAST::ASTNode * enter(MxAST::ASTStatementFor *stat) override;
	virtual MxAST::ASTNode * leave(MxAST::ASTStatementFor *stat) override;
protected:
	MemberTable *memTable;
	IssueCollector *issues;
	GlobalSymbol *symbols;

	std::map<size_t, std::map<size_t, size_t>> mapClassMemberId;	//class name -> { member name -> var id }
	std::map<size_t, size_t> mapGlobalVar;

	std::vector<MemberTable::varInfo> vLocalVar;
	std::map<size_t, std::stack<size_t>> mapLocalVar;		//name -> local var id
	std::stack<std::set<size_t>> stkCurrentBlockVar;

	size_t curClass, curFunc;
	size_t depthLoop;
};

#endif