#ifndef MX_COMPILER_VARIABLE_CHECKER_H
#define MX_COMPILER_VARIABLE_CHECKER_H

#include "MxProgram.h"
#include "IssueCollector.h"
#include "GlobalSymbol.h"
#include "AST.h"

class StaticTypeChecker : public MxAST::ASTListener
{
public:
	StaticTypeChecker(MxProgram *memTable, GlobalSymbol *symbols, IssueCollector *issues);
	bool preCheck(MxAST::ASTRoot *root);

protected:
	bool checkFunc(MxAST::ASTDeclFunc *declFunc, 
		std::map<size_t, size_t> &mapVarId, std::vector<MxProgram::varInfo> &varTable,
		size_t className);
	bool checkVar(MxAST::ASTDeclVar *declVar, std::map<size_t, size_t> &mapVarId, std::vector<MxProgram::varInfo> &varTable);
	bool checkType(MxType type, ssize_t tokenL, ssize_t tokenR);

	size_t findConstructor(size_t className, const std::vector<MxType> &vTypes);
	size_t findOverloadedFunc(size_t olid, const std::vector<MxType> &vTypes); //return -1 for not found, -2 for ambiguous call

	virtual MxAST::ASTNode * enter(MxAST::ASTDeclClass *declClass) override;
	virtual MxAST::ASTNode * leave(MxAST::ASTDeclClass *declClass) override;
	virtual MxAST::ASTNode * enter(MxAST::ASTDeclFunc *declFunc) override;
	virtual MxAST::ASTNode * leave(MxAST::ASTDeclFunc *declFunc) override;
	virtual MxAST::ASTNode * enter(MxAST::ASTDeclVarGlobal *declVar) override;
	virtual MxAST::ASTNode * enter(MxAST::ASTDeclVarLocal *declVar) override;
	virtual MxAST::ASTNode * leave(MxAST::ASTDeclVar *declVar) override;
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
	MxProgram *program;
	IssueCollector *issues;
	GlobalSymbol *symbols;

	std::map<size_t, std::map<size_t, size_t>> mapClassMemberId;	//class name -> { member name -> var id }
	std::map<size_t, size_t> mapGlobalVar;

	std::vector<MxProgram::varInfo> vLocalVar;
	std::map<size_t, std::stack<size_t>> mapLocalVar;		//name -> local var id
	std::stack<std::set<size_t>> stkCurrentBlockVar;

	size_t curClass, curFunc;
	size_t depthLoop;
};

#endif