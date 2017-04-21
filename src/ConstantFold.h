#ifndef MX_COMPILER_CONSTANT_FOLDER_H
#define MX_COMPILER_CONSTANT_FOLDER_H

#include "AST.h"
#include "IssueCollector.h"
#include "GlobalSymbol.h"

namespace ASTOptimizer
{
	class ConstantFold : public MxAST::ASTListener
	{
	public:
		ConstantFold() : issues(IssueCollector::getDefault()), symbols(GlobalSymbol::getDefault()) {}
		ConstantFold(IssueCollector *issues, GlobalSymbol *symbols) : issues(issues), symbols(symbols) {}

		virtual MxAST::ASTNode * leave(MxAST::ASTExprUnary *expr) override;
		virtual MxAST::ASTNode * leave(MxAST::ASTExprBinary *expr) override;

	protected:
		IssueCollector *issues;
		GlobalSymbol *symbols;
	};
}

#endif