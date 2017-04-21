#include "ASTConstructor.h"
#include <sstream>
using namespace MxAST;

ASTRoot * ASTConstructor::constructAST(MxParser::ProgContext *prog, GlobalSymbol *symbolTable)
{
	symbols = symbolTable;
	curClass = -1;
	visit(prog);
	ASTRoot *root = dynamic_cast<ASTRoot *>(node.release());
	assert(root);
	return root;
}

MxType ASTConstructor::getType(MxParser::TypeNotArrayContext *ctx)
{
	MxType type;
	if (ctx->typeInternal())
	{
		if (ctx->typeInternal()->BoolType())
			type.mainType = MxType::Bool;
		else if (ctx->typeInternal()->IntType())
			type.mainType = MxType::Integer;
		else if (ctx->typeInternal()->StringType())
			type.mainType = MxType::String;
		else
		{
			assert(false);
		}
	}
	else if (ctx->Id())
	{
		type.mainType = MxType::Object;
		std::string className = ctx->Id()->toString();
		type.className = symbols->addSymbol(className);
		IF_DEBUG(type.strClassName = className);
	}
	type.arrayDim = 0;
	return type;
}
MxType ASTConstructor::getType(MxParser::TypeContext *ctx)
{
	MxType type = getType(ctx->typeNotArray());
	type.arrayDim = ctx->OpenSqu().size();
	return type;
}


std::vector<std::unique_ptr<ASTNode>> ASTConstructor::getParamList(MxParser::ParamListContext *ctx)
{
	std::vector<std::unique_ptr<ASTNode>> ret;
	auto types = ctx->type();
	auto ids = ctx->Id();
	assert(types.size() == ids.size());
	for (size_t i = 0; i < types.size(); i++)
	{
		std::string strVarName = ids[i]->toString();
		std::unique_ptr<ASTDeclVar> var(new ASTDeclVarLocal);
		var->tokenL = types[i]->getSourceInterval().a;
		var->tokenR = ids[i]->getSourceInterval().b;
		var->varType = getType(types[i]);
		var->varName = symbols->addSymbol(strVarName);
		IF_DEBUG(var->strVarName = strVarName);
		ret.emplace_back(var.release());
	}
	return ret;
}

std::vector<std::unique_ptr<ASTNode>> ASTConstructor::getVarDecl(MxParser::VarDeclContext *ctx, bool isGlobal)
{
	std::vector<std::unique_ptr<ASTNode>> ret;
	MxType type = getType(ctx->type());
	std::unique_ptr<ASTDeclVar> var;
	for (size_t i = 1; i < ctx->children.size(); i++)
	{
		antlr4::tree::TerminalNode *term = dynamic_cast<antlr4::tree::TerminalNode *>(ctx->children[i]);
		if (term && term->getSymbol()->getType() == MxParser::Id)
		{
			std::string strVarName = term->toString();
			if (var)
				ret.emplace_back(var.release());
			var.reset(isGlobal ?
				static_cast<ASTDeclVar *>(newNode<ASTDeclVarGlobal>(term)) :
				static_cast<ASTDeclVar *>(newNode<ASTDeclVarLocal>(term)));
			var->varType = type;
			var->varName = symbols->addSymbol(strVarName);
			IF_DEBUG(var->strVarName = strVarName);
		}
		else if (dynamic_cast<MxParser::ExprContext *>(ctx->children[i]))
		{
			assert(var);
			visit(ctx->children[i]);
			if (node)
			{
				assert(dynamic_cast<ASTExpr *>(node.get()));
				var->initVal = std::move(node);
			}
		}
	}
	assert(var);
	ret.emplace_back(var.release());
	return ret;
}

std::vector<std::unique_ptr<ASTNode>> ASTConstructor::getStatment(MxParser::StatementContext *ctx)
{
	std::vector<std::unique_ptr<ASTNode>> ret;
	if (ctx->block() || ctx->if_statement() || ctx->for_statement() || ctx->while_statement())
	{
		assert(ctx->children.size() == 1);
		visit(ctx->children[0]);
		if (node)
		{
			assert(dynamic_cast<ASTStatement *>(node.get()));
			ret.emplace_back(std::move(node));
		}
	}
	else if (ctx->varDecl())
		ret = getVarDecl(ctx->varDecl(), false);
	else
	{
		assert(ctx->Semicolon());
		if (ctx->Continue())
			ret.emplace_back(newNode<ASTStatementContinue>(ctx->Continue()));
		else if (ctx->Break())
			ret.emplace_back(newNode<ASTStatementBreak>(ctx->Break()));
		else if (ctx->Return())
		{
			std::unique_ptr<ASTStatementReturn> stat(newNode<ASTStatementReturn>(ctx->Return()));
			if (ctx->expr())
			{
				visit(ctx->expr());
				if (node)
				{
					assert(dynamic_cast<ASTExpr *>(node.get()));
					stat->retExpr = std::move(node);
				}
			}
			ret.emplace_back(stat.release());
		}
		else if (ctx->expr())
		{
			std::unique_ptr<ASTStatementExpr> stat(newNode<ASTStatementExpr>(ctx->expr()));
			visit(ctx->expr());
			if (node)
			{
				assert(dynamic_cast<ASTExpr *>(node.get()));
				stat->expr = std::move(node);
				ret.emplace_back(stat.release());
			}
		}
	}
	return ret;
}

std::vector<std::unique_ptr<ASTNode>> ASTConstructor::getExprList(MxParser::ExprListContext *ctx)
{
	std::vector<std::unique_ptr<ASTNode>> ret;
	for (auto &expr : ctx->expr())
	{
		visit(expr);
		if (node)
		{
			assert(dynamic_cast<ASTExpr *>(node.get()));
			ret.emplace_back(std::move(node));
		}
	}
	return ret;
}

antlrcpp::Any ASTConstructor::visitPrefixUnaryExpr(antlr4::ParserRuleContext *ctx, ASTExprUnary::Operator oper)
{
	visit(ctx->children[1]);
	if (node)
	{
		assert(dynamic_cast<ASTExpr *>(node.get()));
		node.reset(newNode<ASTExprUnary>(ctx, oper, node.release()));
	}
	return nullptr;
}

antlrcpp::Any ASTConstructor::visitBinaryExpr(antlr4::ParserRuleContext *ctx, ASTExprBinary::Operator oper)
{
	std::unique_ptr<ASTNode> exprL, exprR;
	visit(ctx->children[0]);
	if (node)
	{
		assert(dynamic_cast<ASTExpr *>(node.get()));
		exprL = std::move(node);
	}
	visit(ctx->children[2]);
	if (node)
	{
		assert(dynamic_cast<ASTExpr *>(node.get()));
		exprR = std::move(node);
	}
	node.reset(newNode<ASTExprBinary>(ctx, oper, std::move(exprL), std::move(exprR)));
	return nullptr;
}

antlrcpp::Any ASTConstructor::visitBlock(MxParser::BlockContext *ctx)
{
	std::unique_ptr<ASTBlock> block(newNode<ASTStatementBlock>(ctx));
	for (auto &statctx : ctx->statement())
	{
		auto stats = getStatment(statctx);
		for (auto &stat : stats)
			block->stats.emplace_back(std::move(stat));
	}
	node.reset(block.release());
	return nullptr;
}

antlrcpp::Any ASTConstructor::visitProg(MxParser::ProgContext *ctx)
{
	std::unique_ptr<ASTRoot> root(newNode<ASTRoot>(ctx));
	for (auto &child : ctx->children)
	{
		if (dynamic_cast<MxParser::VarDeclContext *>(child))
		{
			auto vars = getVarDecl(dynamic_cast<MxParser::VarDeclContext *>(child), true);
			for (auto &var : vars)
				root->nodes.emplace_back(std::move(var));
		}
		else
		{
			visit(child);
			if (node)
			{
				assert(dynamic_cast<ASTDeclClass *>(node.get()) || dynamic_cast<ASTDeclFunc *>(node.get()));
				root->nodes.emplace_back(std::move(node));
			}
		}
	}
	node.reset(root.release());
	return nullptr;
}

antlrcpp::Any ASTConstructor::visitClassDecl(MxParser::ClassDeclContext *ctx)
{
	std::unique_ptr<ASTDeclClass> declClass(newNode<ASTDeclClass>(ctx));
	std::string className = ctx->Id()->toString();
	declClass->className = symbols->addSymbol(className);
	curClass = declClass->className;
	IF_DEBUG(declClass->strClassName = className);
	IF_DEBUG(strCurClass = className);

	for (auto &child : ctx->memberList()->varDecl())
	{
		auto vars = getVarDecl(child, true);
		for (auto &var : vars)
			declClass->vMemVars.emplace_back(std::move(var));
	}
	for (auto &child : ctx->memberList()->funcDecl())
	{
		visit(child);
		if (node)
		{
			assert(dynamic_cast<ASTDeclFunc *>(node.get()));
			declClass->vMemFuncs.emplace_back(std::move(node));
		}
	}

	curClass = -1;
	IF_DEBUG(strCurClass = "");
	node.reset(declClass.release());
	return nullptr;
}

antlrcpp::Any ASTConstructor::visitFuncDecl(MxParser::FuncDeclContext *ctx)
{
	std::string strFuncName = ctx->Id()->toString();
	size_t funcName = symbols->addSymbol(strFuncName);
	MxType retType{ MxType::Void };
	if (!ctx->type() && !ctx->Void())
	{
		if (funcName != curClass)
		{
			issues->error(ctx->Id()->getSourceInterval().a, ctx->Id()->getSourceInterval().b,
				"Function declaration without return type");
			node.reset();
			return nullptr;
		}
	}
	else if (ctx->type())
		retType = getType(ctx->type());

	std::unique_ptr<ASTDeclFunc> declFunc(newNode<ASTDeclFunc>(ctx));
	declFunc->funcName = funcName;
	IF_DEBUG(declFunc->strFuncName = strFuncName);
	declFunc->param = getParamList(ctx->paramList());
	declFunc->retType = retType;

	visit(ctx->block());
	if (node)
	{
		ASTBlock *block = dynamic_cast<ASTBlock *>(node.get());
		assert(block);
		declFunc->stats = std::move(block->stats);
	}

	node.reset(declFunc.release());
	return nullptr;
}

antlrcpp::Any ASTConstructor::visitIf_statement(MxParser::If_statementContext *ctx)
{
	std::unique_ptr<ASTStatementIf> stat(newNode<ASTStatementIf>(ctx));
	visit(ctx->expr());
	if (node)
	{
		assert(dynamic_cast<ASTExpr *>(node.get()));
		stat->exprCond = std::move(node);
	}
	auto ifStat = getStatment(ctx->statement(0));
	if (!ifStat.empty())
	{
		std::unique_ptr<ASTStatementBlock> block(newNode<ASTStatementBlock>(ctx->statement(0)));
		block->stats = std::move(ifStat);
		stat->ifStat.reset(block.release());
	}
	if (ctx->Else())
	{
		assert(ctx->statement().size() == 2);
		auto elseStat = getStatment(ctx->statement(1));
		if (!elseStat.empty())
		{
			std::unique_ptr<ASTStatementBlock> block(newNode<ASTStatementBlock>(ctx->statement(1)));
			block->stats = std::move(elseStat);
			stat->elseStat.reset(block.release());
		}
	}
	node.reset(stat.release());
	return nullptr;
}

antlrcpp::Any ASTConstructor::visitWhile_statement(MxParser::While_statementContext *ctx)
{
	std::unique_ptr<ASTStatementWhile> stat(newNode<ASTStatementWhile>(ctx));
	visit(ctx->expr());
	if (node)
	{
		assert(dynamic_cast<ASTExpr *>(node.get()));
		stat->exprCond = std::move(node);
	}
	stat->stats = getStatment(ctx->statement());
	node.reset(stat.release());
	return nullptr;
}

antlrcpp::Any ASTConstructor::visitFor_statement(MxParser::For_statementContext *ctx)
{
	std::unique_ptr<ASTStatementFor> stat(newNode<ASTStatementFor>(ctx));
	if (ctx->for_exprIn()->expr())
	{
		visit(ctx->for_exprIn()->expr());
		if (node)
		{
			assert(dynamic_cast<ASTExpr *>(node.get()));
			std::unique_ptr<ASTStatementExpr> exprIn(newNode<ASTStatementExpr>(ctx->for_exprIn()));
			exprIn->expr = std::move(node);
			stat->exprIn.reset(exprIn.release());
		}
	}
	else if (ctx->for_exprIn()->varDecl())
		stat->varDecls = getVarDecl(ctx->for_exprIn()->varDecl(), false);

	if (ctx->for_exprCond()->expr())
	{
		visit(ctx->for_exprCond()->expr());
		if (node)
		{
			assert(dynamic_cast<ASTExpr *>(node.get()));
			stat->exprCond = std::move(node);
		}
	}

	if (ctx->for_exprStep()->expr())
	{
		visit(ctx->for_exprStep()->expr());
		if (node)
		{
			assert(dynamic_cast<ASTExpr *>(node.get()));
			std::unique_ptr<ASTStatementExpr> exprStep(newNode<ASTStatementExpr>(ctx->for_exprStep()));
			exprStep->expr = std::move(node);
			stat->exprStep.reset(exprStep.release());
		}
	}

	stat->stats = getStatment(ctx->statement());
	node.reset(stat.release());
	return nullptr;
}

antlrcpp::Any ASTConstructor::visitExprPrimary(MxParser::ExprPrimaryContext *ctx)
{
	if (ctx->Id())
	{
		std::string strVarName = ctx->Id()->toString();
		std::unique_ptr<ASTExprVar> var(newNode<ASTExprVar>(ctx->Id()));
		var->isThis = false;
		var->varName = symbols->addSymbol(strVarName);
		IF_DEBUG(var->strVarName = strVarName);
		node.reset(var.release());
	}
	else if (ctx->String())
	{
		std::string strContent = ctx->String()->toString();
		size_t length = strContent.size();
		assert(length >= 2 && strContent[0] == '\"' && strContent[length - 1] == '\"');
		strContent = strContent.substr(1, length - 2);
		strContent = transferString(strContent, ctx->getSourceInterval().a, ctx->getSourceInterval().b);
		
		std::unique_ptr<ASTExprImm> imm(newNode<ASTExprImm>(ctx->String()));
		imm->exprType = MxType{ MxType::String };
		imm->exprVal.strId = symbols->addString(strContent);
		IF_DEBUG(imm->strContent = strContent);
		node.reset(imm.release());
	}
	else if (ctx->IntegerDec())
	{
		std::stringstream ss(ctx->IntegerDec()->toString());
		int value;
		ss >> value;
		node.reset(newNode<ASTExprImm>(ctx->IntegerDec(), value));
	}
	else if (ctx->True())
		node.reset(newNode<ASTExprImm>(ctx->True(), true));
	else if (ctx->False())
		node.reset(newNode<ASTExprImm>(ctx->False(), false));
	else if (ctx->This())
	{
		std::unique_ptr<ASTExprVar> var(newNode<ASTExprVar>(ctx->This()));
		var->isThis = true;
		var->varName = -1;
		node.reset(var.release());
	}
	else if (ctx->Null())
		node.reset(newNode<ASTExprImm>(ctx->Null(), nullptr));
	else
	{
		assert(ctx->exprPar());
		visit(ctx->exprPar()->expr());
	}
	return nullptr;
}

antlrcpp::Any ASTConstructor::visitExprIncrementPostfix(MxParser::ExprIncrementPostfixContext *ctx)
{
	visit(ctx->subexprPostfix());
	if (node)
	{
		assert(dynamic_cast<ASTExpr *>(node.get()));
		node.reset(newNode<ASTExprUnary>(ctx, ASTExprUnary::IncPostfix, node.release()));
	}
	return nullptr;
}

antlrcpp::Any ASTConstructor::visitExprDecrementPostfix(MxParser::ExprDecrementPostfixContext *ctx)
{
	visit(ctx->subexprPostfix());
	if (node)
	{
		assert(dynamic_cast<ASTExpr *>(node.get()));
		node.reset(newNode<ASTExprUnary>(ctx, ASTExprUnary::DecPostfix, node.release()));
	}
	return nullptr;
}

antlrcpp::Any ASTConstructor::visitExprMember(MxParser::ExprMemberContext *ctx)
{
	visit(ctx->subexprPostfix());
	if (node)
	{
		assert(dynamic_cast<ASTExpr *>(node.get()));
		std::string strMemberName = ctx->Id()->toString();
		std::unique_ptr<ASTExprMemberAccess> expr(newNode<ASTExprMemberAccess>(ctx));
		expr->object = std::move(node);
		expr->memberName = symbols->addSymbol(strMemberName);
		IF_DEBUG(expr->strMemberName = strMemberName);
		node.reset(expr.release());
	}
	return nullptr;
}

antlrcpp::Any ASTConstructor::visitExprFuncCall(MxParser::ExprFuncCallContext *ctx)
{
	visit(ctx->subexprPostfix());
	if (node)
	{
		assert(dynamic_cast<ASTExpr *>(node.get()));
		std::unique_ptr<ASTExprFuncCall> funcCall(newNode<ASTExprFuncCall>(ctx));
		funcCall->func = std::move(node);
		funcCall->paramList = getExprList(ctx->exprList());
		node.reset(funcCall.release());
	}
	return nullptr;
}

antlrcpp::Any ASTConstructor::visitExprSubscript(MxParser::ExprSubscriptContext *ctx)
{
	visit(ctx->subexprPostfix());
	if (node)
	{
		assert(dynamic_cast<ASTExpr *>(node.get()));
		std::unique_ptr<ASTExprSubscriptAccess> sub(newNode<ASTExprSubscriptAccess>(ctx));
		sub->arr = std::move(node);
		visit(ctx->expr());
		if (node)
		{
			assert(dynamic_cast<ASTExpr *>(node.get()));
			sub->subscript = std::move(node);
		}
		node.reset(sub.release());
	}
	return nullptr;
}

antlrcpp::Any ASTConstructor::visitExprNew(MxParser::ExprNewContext *ctx)
{
	MxType type = getType(ctx->typeNotArray());
	std::unique_ptr<ASTExprNew> exprNew(newNode<ASTExprNew>(ctx));
	if (ctx->OpenPar())
	{
		exprNew->isArray = false;
		exprNew->paramList = getExprList(ctx->exprList());
		exprNew->exprType = type;
	}
	else
	{
		bool isEmpty = false;
		for (auto &dim : ctx->exprNewDim())
		{
			if (!dim->expr())
			{
				isEmpty = true;
				exprNew->paramList.emplace_back(nullptr);
			}
			else
			{
				if (isEmpty)
				{
					issues->error(dim->getSourceInterval().a, dim->getSourceInterval().b,
						"Unexpected expression");
					node.reset(nullptr);
					return nullptr;
				}
				visit(dim->expr());
				if (node)
				{
					assert(dynamic_cast<ASTExpr *>(node.get()));
					exprNew->paramList.emplace_back(std::move(node));
				}
			}
		}
		exprNew->isArray = ctx->exprNewDim().size() > 0;
		exprNew->exprType = type;
		exprNew->exprType.arrayDim = ctx->exprNewDim().size();
	}
	node.reset(exprNew.release());
	return nullptr;
}

antlrcpp::Any ASTConstructor::visitExprAssignment(MxParser::ExprAssignmentContext *ctx)
{
	std::unique_ptr<ASTExprAssignment> assign(newNode<ASTExprAssignment>(ctx));
	visit(ctx->subexprOr());
	if (node)
	{
		assert(dynamic_cast<ASTExpr *>(node.get()));
		assign->operandL = std::move(node);
	}
	visit(ctx->subexprAssignment());
	if (node)
	{
		assert(dynamic_cast<ASTExpr *>(node.get()));
		assign->operandR = std::move(node);
	}
	node.reset(assign.release());
	return nullptr;
}

std::string ASTConstructor::transferString(const std::string &in, ssize_t tokenL, ssize_t tokenR)
{
	std::string ret;
	bool transfer = false;
	for (size_t i = 0; i < in.size(); i++)
	{
		if (transfer)
		{
			if (in[i] == 'n')
				ret += '\n';
			else if (in[i] == 'r')
				ret += '\r';
			else if (in[i] == 't')
				ret += '\t';
			else if (in[i] == '\\')
				ret += '\\';
			else if (in[i] == '"')
				ret += '"';
			else if (in[i] == '\'')
				ret += '\'';
			else
				issues->error(tokenL, tokenR, std::string("unrecognized transfer character: ") + in[i]);
			transfer = false;
			continue;
		}
		if (in[i] == '\\')
			transfer = true;
		else
			ret += in[i];
	}
	return ret;
}