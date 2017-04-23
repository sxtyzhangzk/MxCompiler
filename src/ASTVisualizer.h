#ifndef MX_COMPILER_AST_VISUALIZER_H
#define MX_COMPILER_AST_VISUALIZER_H

#include "AST.h"
#include "GlobalSymbol.h"

class ASTVisualizer : public MxAST::ASTVisitor
{
public:
	ASTVisualizer(std::ostream &out, GlobalSymbol &symbol) : out(out), cntNode(0), symbol(symbol), inClass(false) {}
	void printHead();
	void printFoot();

	//virtual void visit(MxAST::ASTNode *node) override;
	virtual void visit(MxAST::ASTRoot *root) override;
	virtual void visit(MxAST::ASTDeclClass *declClass) override;
	virtual void visit(MxAST::ASTDeclFunc *declFunc) override;
	virtual void visit(MxAST::ASTDeclVar *declVar) override;
	virtual void visit(MxAST::ASTExprImm *expr) override;
	virtual void visit(MxAST::ASTExprVar *expr) override;
	virtual void visit(MxAST::ASTExprUnary *expr) override;
	virtual void visit(MxAST::ASTExprBinary *expr) override;
	virtual void visit(MxAST::ASTExprAssignment *expr) override;
	virtual void visit(MxAST::ASTExprNew *expr) override;
	virtual void visit(MxAST::ASTExprSubscriptAccess *expr) override;
	virtual void visit(MxAST::ASTExprMemberAccess *expr) override;
	virtual void visit(MxAST::ASTExprFuncCall *expr) override;
	virtual void visit(MxAST::ASTStatementReturn *stat) override;
	virtual void visit(MxAST::ASTStatementBreak *stat) override;
	virtual void visit(MxAST::ASTStatementContinue *stat) override;
	virtual void visit(MxAST::ASTStatementIf *stat) override;
	virtual void visit(MxAST::ASTStatementWhile *stat) override;
	virtual void visit(MxAST::ASTStatementFor *stat) override;
	virtual void visit(MxAST::ASTStatementBlock *stat) override;
	virtual void visit(MxAST::ASTStatementExpr *stat) override;
	

protected:
	std::string type2html(MxType type);
	//static std::string transferHTML(const std::string &in);
	static std::string exprColor(MxAST::ASTExpr *expr);

protected:
	bool inClass;
	std::ostream &out;
	GlobalSymbol &symbol;
	size_t cntNode, lastNode;
};

#endif