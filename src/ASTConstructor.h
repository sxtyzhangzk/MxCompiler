#ifndef MX_COMPILER_AST_CONSTRUCTOR_H
#define MX_COMPILER_AST_CONSTRUCTOR_H

#include "AST.h"
#include "GlobalSymbol.h"
#include "IssueCollector.h"
#include <MxParserBaseVisitor.h>
#include <memory>

class ASTConstructor : protected MxParserBaseVisitor
{
public:
	ASTConstructor(IssueCollector *issues) : issues(issues) {}
	MxAST::ASTRoot * constructAST(MxParser::ProgContext *prog, GlobalSymbol *symbolTable);

protected:
	template<typename T, typename... Tparam>
	static T * newNode(antlr4::tree::ParseTree *tree, Tparam&&... param)
	{
		T *ret = new T(std::forward<Tparam>(param)...);
		ret->tokenL = tree->getSourceInterval().a;
		ret->tokenR = tree->getSourceInterval().b;
		return ret;
	}
	antlrcpp::Any visitPrefixUnaryExpr(antlr4::ParserRuleContext *ctx, MxAST::ASTExprUnary::Operator oper);
	antlrcpp::Any ASTConstructor::visitBinaryExpr(antlr4::ParserRuleContext *ctx, MxAST::ASTExprBinary::Operator oper);

	MxType getType(MxParser::TypeNotArrayContext *ctx);
	MxType getType(MxParser::TypeContext *ctx);
	std::vector<std::unique_ptr<MxAST::ASTNode>> getParamList(MxParser::ParamListContext *ctx);
	std::vector<std::unique_ptr<MxAST::ASTNode>> getVarDecl(MxParser::VarDeclContext *ctx, bool isGlobal);
	std::vector<std::unique_ptr<MxAST::ASTNode>> getStatment(MxParser::StatementContext *ctx);
	std::vector<std::unique_ptr<MxAST::ASTNode>> getExprList(MxParser::ExprListContext *ctx);

	virtual antlrcpp::Any visitBlock(MxParser::BlockContext *ctx) override;
	virtual antlrcpp::Any visitProg(MxParser::ProgContext *ctx) override;
	virtual antlrcpp::Any visitClassDecl(MxParser::ClassDeclContext *ctx) override;
	virtual antlrcpp::Any visitFuncDecl(MxParser::FuncDeclContext *ctx) override;
	virtual antlrcpp::Any visitVarDecl(MxParser::VarDeclContext *ctx) override { assert(false); return nullptr; }
	virtual antlrcpp::Any visitParamList(MxParser::ParamListContext *ctx) override { assert(false); return nullptr; }
	virtual antlrcpp::Any visitStatement(MxParser::StatementContext *ctx) override { assert(false); return nullptr; }
	virtual antlrcpp::Any visitIf_statement(MxParser::If_statementContext *ctx) override;
	virtual antlrcpp::Any visitWhile_statement(MxParser::While_statementContext *ctx) override;
	virtual antlrcpp::Any visitFor_statement(MxParser::For_statementContext *ctx) override;
	virtual antlrcpp::Any visitExprPrimary(MxParser::ExprPrimaryContext *ctx) override;
	virtual antlrcpp::Any visitExprIncrementPostfix(MxParser::ExprIncrementPostfixContext *ctx) override;
	virtual antlrcpp::Any visitExprDecrementPostfix(MxParser::ExprDecrementPostfixContext *ctx) override;
	virtual antlrcpp::Any visitExprMember(MxParser::ExprMemberContext *ctx) override;
	virtual antlrcpp::Any visitExprFuncCall(MxParser::ExprFuncCallContext *ctx) override;
	virtual antlrcpp::Any visitExprSubscript(MxParser::ExprSubscriptContext *ctx) override;
	virtual antlrcpp::Any visitExprIncrementPrefix(MxParser::ExprIncrementPrefixContext *ctx) override { return visitPrefixUnaryExpr(ctx, MxAST::ASTExprUnary::Increment); }
	virtual antlrcpp::Any visitExprDecrementPrefix(MxParser::ExprDecrementPrefixContext *ctx) override { return visitPrefixUnaryExpr(ctx, MxAST::ASTExprUnary::Decrement); }
	virtual antlrcpp::Any visitExprPositive(MxParser::ExprPositiveContext *ctx) override { return visitPrefixUnaryExpr(ctx, MxAST::ASTExprUnary::Positive); }
	virtual antlrcpp::Any visitExprNegative(MxParser::ExprNegativeContext *ctx) override { return visitPrefixUnaryExpr(ctx, MxAST::ASTExprUnary::Negative); }
	virtual antlrcpp::Any visitExprNot(MxParser::ExprNotContext *ctx) override { return visitPrefixUnaryExpr(ctx, MxAST::ASTExprUnary::Not); }
	virtual antlrcpp::Any visitExprBitNot(MxParser::ExprBitNotContext *ctx) override { return visitPrefixUnaryExpr(ctx, MxAST::ASTExprUnary::BitNot); }
	virtual antlrcpp::Any visitExprNew(MxParser::ExprNewContext *ctx) override;
	virtual antlrcpp::Any visitExprMulti(MxParser::ExprMultiContext *ctx) override { return visitBinaryExpr(ctx, MxAST::ASTExprBinary::Multiple); }
	virtual antlrcpp::Any visitExprDiv(MxParser::ExprDivContext *ctx) override { return visitBinaryExpr(ctx, MxAST::ASTExprBinary::Divide); }
	virtual antlrcpp::Any visitExprMod(MxParser::ExprModContext *ctx) override { return visitBinaryExpr(ctx, MxAST::ASTExprBinary::Mod); }
	virtual antlrcpp::Any visitExprPlus(MxParser::ExprPlusContext *ctx) override { return visitBinaryExpr(ctx, MxAST::ASTExprBinary::Plus); }
	virtual antlrcpp::Any visitExprMinus(MxParser::ExprMinusContext *ctx) override { return visitBinaryExpr(ctx, MxAST::ASTExprBinary::Minus); }
	virtual antlrcpp::Any visitExprShiftLeft(MxParser::ExprShiftLeftContext *ctx) override { return visitBinaryExpr(ctx, MxAST::ASTExprBinary::ShiftLeft); }
	virtual antlrcpp::Any visitExprShiftRight(MxParser::ExprShiftRightContext *ctx) override { return visitBinaryExpr(ctx, MxAST::ASTExprBinary::ShiftRight); }
	virtual antlrcpp::Any visitExprLessThan(MxParser::ExprLessThanContext *ctx) override { return visitBinaryExpr(ctx, MxAST::ASTExprBinary::LessThan); }
	virtual antlrcpp::Any visitExprLessEqual(MxParser::ExprLessEqualContext *ctx) override { return visitBinaryExpr(ctx, MxAST::ASTExprBinary::LessEqual); }
	virtual antlrcpp::Any visitExprGreaterThan(MxParser::ExprGreaterThanContext *ctx) override { return visitBinaryExpr(ctx, MxAST::ASTExprBinary::GreaterThan); }
	virtual antlrcpp::Any visitExprGreaterEqual(MxParser::ExprGreaterEqualContext *ctx) override { return visitBinaryExpr(ctx, MxAST::ASTExprBinary::GreaterEqual); }
	virtual antlrcpp::Any visitExprEqual(MxParser::ExprEqualContext *ctx) override { return visitBinaryExpr(ctx, MxAST::ASTExprBinary::Equal); }
	virtual antlrcpp::Any visitExprNotEqual(MxParser::ExprNotEqualContext *ctx) override { return visitBinaryExpr(ctx, MxAST::ASTExprBinary::NotEqual); }
	virtual antlrcpp::Any visitExprBitand(MxParser::ExprBitandContext *ctx) override { return visitBinaryExpr(ctx, MxAST::ASTExprBinary::BitAnd); }
	virtual antlrcpp::Any visitExprXor(MxParser::ExprXorContext *ctx) override { return visitBinaryExpr(ctx, MxAST::ASTExprBinary::BitXor); }
	virtual antlrcpp::Any visitExprBitor(MxParser::ExprBitorContext *ctx) override { return visitBinaryExpr(ctx, MxAST::ASTExprBinary::BitOr); }
	virtual antlrcpp::Any visitExprAnd(MxParser::ExprAndContext *ctx) override { return visitBinaryExpr(ctx, MxAST::ASTExprBinary::And); }
	virtual antlrcpp::Any visitExprOr(MxParser::ExprOrContext *ctx) override { return visitBinaryExpr(ctx, MxAST::ASTExprBinary::Or); }
	virtual antlrcpp::Any visitExprAssignment(MxParser::ExprAssignmentContext *ctx) override;

protected:
	IssueCollector *issues;
	GlobalSymbol *symbols;
	std::unique_ptr<MxAST::ASTNode> node;
	size_t curClass;
	IF_DEBUG(std::string strCurClass);
};

#endif