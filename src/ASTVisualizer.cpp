#include "common_headers.h"
#include "ASTVisualizer.h"
using namespace MxAST;

void ASTVisualizer::printHead()
{
	out << "digraph mxprog {" << std::endl;
}

void ASTVisualizer::printFoot()
{
	out << "}" << std::endl;
}

std::string ASTVisualizer::type2html(MxType type)
{
	std::string ret;
	switch (type.mainType)
	{
	case MxType::Void:
		ret = "<font color='#569cd6'>void</font>";
		break;
	case MxType::Bool:
		ret = "<font color='#569cd6'>bool</font>";
		break;
	case MxType::Integer:
		ret = "<font color='#569cd6'>int</font>";
		break;
	case MxType::String:
		ret = "<font color='#569cd6'>string</font>";
		break;
	case MxType::Object:
		if (type.className == -1)
			ret = "<font color='#4ec9b0'>nullptr_t</font>";
		else
		{
			ret = "<font color='#4ec9b0'>";
			ret += symbol.vSymbol[type.className];
			ret += "</font>";
		}
		break;
	case MxType::Function:
		ret = "<font color='#569cd6'>function</font>";
		break;
	default:
		assert(false);
	}
	for (size_t i = 0; i < type.arrayDim; i++)
		ret += "[]";
	return ret;
}

//void ASTVisualizer::visit(MxAST::ASTNode *node)
//{
//	size_t idx = cntNode++;
//	out << idx << " [label=\"unknown\"];" << std::endl;
//	lastNode = idx;
//}

void ASTVisualizer::visit(ASTRoot *root)
{
	size_t idx = cntNode++;
	out << idx << " [shape=box,label=\"root\"];" << std::endl;
	for (auto &node : root->nodes)
	{
		node->accept(this);
		out << idx << " -> " << lastNode << ";" << std::endl;
	}
	lastNode = idx;
}

void ASTVisualizer::visit(ASTDeclClass *declClass)
{
	size_t idx = cntNode++;
	out << "subgraph \"cluster_class_" << symbol.vSymbol[declClass->className] << "\" {" << std::endl;
	out << idx << " [shape=none,label=<";
	out << "<table border='0' cellborder='1' cellspacing='0'>";
	out << "<tr><td colspan='2'><font face='Consolas'><font color='#569cd6'>class</font> ";
	out << "<font color='#4ec9b0'>" << symbol.vSymbol[declClass->className] << "</font></font></td></tr>";
	out << "<tr><td port='var'>variables</td><td port='func'>functions</td></tr>";
	out << "</table>>];" << std::endl;
	inClass = true;
	for (auto &var : declClass->vMemVars)
	{
		var->accept(this);
		out << idx << ":var -> " << lastNode << ";" << std::endl;
	}
	for (auto &func : declClass->vMemFuncs)
	{
		func->accept(this);
		out << idx << ":func -> " << lastNode << ";" << std::endl;
	}
	inClass = false;
	out << "}" << std::endl;
	lastNode = idx;
}

void ASTVisualizer::visit(ASTDeclFunc *declFunc)
{
	size_t idx = cntNode++;
	out << "subgraph \"cluster_func_" << declFunc->funcID << "\" {" << std::endl;
	out << idx << " [shape=none,label=<";
	out << "<table border='0' cellborder='1' cellspacing='0'>";
	out << "<tr><td colspan='2'><font face='Consolas'>";
	out << type2html(declFunc->retType) << " ";
	out << symbol.vSymbol[declFunc->funcName] << "(...)";
	out << "</font></td></tr>";
	out << "<tr><td colspan='2'>func id: " << declFunc->funcID << "</td></tr>";
	out << "<tr><td port='param'>parameters</td><td port='content'>content</td></tr>";
	out << "</table>>];" << std::endl;
	for (auto &var : declFunc->param)
	{
		var->accept(this);
		out << idx << ":param -> " << lastNode << ";" << std::endl;
	}
	for (auto &stat : declFunc->stats)
	{
		stat->accept(this);
		out << idx << ":content -> " << lastNode << ";" << std::endl;
	}
	out << "}" << std::endl;
	lastNode = idx;
}

void ASTVisualizer::visit(ASTDeclVar *declVar)
{
	size_t idx = cntNode++;
	out << idx << " [shape=none,label=<";
	out << "<table border='0' cellborder='1' cellspacing='0'>";
	out << "<tr><td><font face='Consolas'>";
	out << type2html(declVar->varType) << " ";
	out << symbol.vSymbol[declVar->varName];
	out << "</font></td></tr>";
	out << "<tr><td>var id: ";
	out << declVar->varID << " ";
	if (dynamic_cast<ASTDeclVarLocal *>(declVar))
		out << "(local)";
	else
		out << (inClass ? "(member)" : "(global)");
	out << "</td></tr>";
	if (declVar->initVal)
	{
		out << "<tr><td port='initval'>init val</td></tr>";
		out << "</table>>];" << std::endl;
		declVar->initVal->accept(this);
		out << idx << ":initval -> " << lastNode << ";" << std::endl;
	}
	else
		out << "</table>>];" << std::endl;
	lastNode = idx;
}

void ASTVisualizer::visit(ASTExprImm *expr)
{
	size_t idx = cntNode++;
	out << idx << " [shape=none,label=<";
	out << "<table border='0' cellborder='1' cellspacing='0' bgcolor='" << exprColor(expr) << "'>";
	out << "<tr><td><font face='Consolas'>";
	switch (expr->exprType.mainType)
	{
	case MxType::Bool:
		out << "<font color='#569cd6'>" << (expr->exprVal.bvalue ? "true" : "false") << "</font>";
		break;
	case MxType::Integer:
		out << expr->exprVal.ivalue;
		break;
	case MxType::String:
		out << "<font color='#d69d85'>\"" << transferHTML(symbol.vString[expr->exprVal.strId]) << "\"</font>";
		break;
	case MxType::Object:
		out << "<font color='#569cd6'>null</font>";
		break;
	default:
		assert(false);
	}
	out << "</font></td></tr>";
	out << "<tr><td><font face='Consolas'>" << type2html(expr->exprType) << "</font></td></tr>";
	out << "</table>>];" << std::endl;
	lastNode = idx;
}

void ASTVisualizer::visit(ASTExprVar *expr)
{
	size_t idx = cntNode++;
	out << idx << " [shape=none,label=<";
	out << "<table border='0' cellborder='1' cellspacing='0' bgcolor='" << exprColor(expr) << "'>";
	if (expr->isThis)
	{
		out << "<tr><td><font face='Consolas' color='#569cd6'>this</font></td></tr>";
		out << "<tr><td><font face='Consolas'>" << type2html(expr->exprType) << "</font></td></tr>";
	}
	else
	{
		out << "<tr><td><font face='Consolas'>" << symbol.vSymbol[expr->varName] << "</font></td></tr>";
		out << "<tr><td><font face='Consolas'>" << type2html(expr->exprType) << "</font></td></tr>";
		out << "<tr><td>var id: " << expr->varID << (expr->isGlobalVar ? "(global)" : "(local)") << "</td></tr>";
	}
	out << "</table>>];" << std::endl;
	lastNode = idx;
}

void ASTVisualizer::visit(ASTExprUnary *expr)
{
	size_t idx = cntNode++;
	out << idx << " [shape=none,label=<";
	out << "<table border='0' cellborder='1' cellspacing='0' bgcolor='" << exprColor(expr) << "'>";
	out << "<tr><td colspan='2'>Unary expr</td></tr>";
	out << "<tr><td colspan='2'><font face='Consolas'>" << type2html(expr->exprType) << "</font></td></tr>";
	if (expr->oper == ASTExprUnary::IncPostfix || expr->oper == ASTExprUnary::DecPostfix)
		out << "<tr><td port='operand'>operand</td><td><font face='Consolas'>" << expr->getOperName() << "</font></td></tr>";
	else
		out << "<tr><td><font face='Consolas'>" << expr->getOperName() << "</font></td><td port='operand'>operand</td></tr>";
	out << "</table>>];" << std::endl;
	expr->operand->accept(this);
	out << idx << ":operand -> " << lastNode << ";" << std::endl;
	lastNode = idx;
}

void ASTVisualizer::visit(ASTExprBinary *expr)
{
	size_t idx = cntNode++;
	out << idx << " [shape=none,label=<";
	out << "<table border='0' cellborder='1' cellspacing='0' bgcolor='" << exprColor(expr) << "'>";
	out << "<tr><td colspan='3'>Binary expr</td></tr>";
	out << "<tr><td colspan='3'><font face='Consolas'>" << type2html(expr->exprType) << "</font></td></tr>";
	out << "<tr><td port='operandL'>operand</td>";
	out << "<td><font face = 'Consolas'>" << transferHTML(expr->getOperName()) << "</font></td>";
	out << "<td port='operandR'>operand</td></tr>";
	out << "</table>>];" << std::endl;
	expr->operandL->accept(this);
	out << idx << ":operandL -> " << lastNode << ";" << std::endl;
	expr->operandR->accept(this);
	out << idx << ":operandR -> " << lastNode << ";" << std::endl;
	lastNode = idx;
}

void ASTVisualizer::visit(ASTExprAssignment *expr)
{
	size_t idx = cntNode++;
	out << idx << " [shape=none,label=<";
	out << "<table border='0' cellborder='1' cellspacing='0' bgcolor='" << exprColor(expr) << "'>";
	out << "<tr><td colspan='3'>Assign expr</td></tr>";
	out << "<tr><td colspan='3'><font face='Consolas'>" << type2html(expr->exprType) << "</font></td></tr>";
	out << "<tr><td port='operandL'>operand</td>";
	out << "<td><font face = 'Consolas'>=</font></td>";
	out << "<td port='operandR'>operand</td></tr>";
	out << "</table>>];" << std::endl;
	expr->operandL->accept(this);
	out << idx << ":operandL -> " << lastNode << ";" << std::endl;
	expr->operandR->accept(this);
	out << idx << ":operandR -> " << lastNode << ";" << std::endl;
	lastNode = idx;
}

void ASTVisualizer::visit(MxAST::ASTExprNew *expr)
{
	size_t idx = cntNode++;
	out << idx << " [shape=none,label=<";
	out << "<table border='0' cellborder='1' cellspacing='0' bgcolor='" << exprColor(expr) << "'>";
	size_t colspan = 2 + (expr->isArray ? expr->exprType.arrayDim : (expr->paramList.empty() ? 0 : 1));
	out << "<tr><td colspan='" << colspan << "'>New expr</td></tr>";
	out << "<tr><td colspan='" << colspan << "'><font face='Consolas'>" << type2html(expr->exprType) << "</font></td></tr>";
	out << "<tr><td><font face='Consolas' color='#569cd6'>new</font></td>";
	MxType baseType = expr->exprType;
	baseType.arrayDim = 0;
	out << "<td><font face='Consolas' color='#569cd6'>" << type2html(baseType) << "</font></td>";
	if (!expr->isArray && !expr->paramList.empty())
	{
		out << "<td port='param'>param</td></tr>";
		out << "</table>>];" << std::endl;
		for (auto &param : expr->paramList)
		{
			param->accept(this);
			out << idx << ":param -> " << lastNode << ";" << std::endl;
		}
	}
	else
	{
		for (size_t i = 0; i < expr->paramList.size(); i++)
			out << "<td port='dim" << i << "'>[]</td>";
		out << "</tr>";
		out << "</table>>];" << std::endl;
		for (size_t i = 0; i < expr->paramList.size(); i++)
		{
			if (!expr->paramList[i])
				continue;
			expr->paramList[i]->accept(this);
			out << idx << ":dim" << i << " -> " << lastNode << ";" << std::endl;
		}
	}
	lastNode = idx;
}

void ASTVisualizer::visit(ASTExprSubscriptAccess *expr)
{
	size_t idx = cntNode++;
	out << idx << " [shape=none,label=<";
	out << "<table border='0' cellborder='1' cellspacing='0' bgcolor='" << exprColor(expr) << "'>";
	out << "<tr><td colspan='2'>Subscript expr</td></tr>";
	out << "<tr><td colspan='2'><font face='Consolas'>" << type2html(expr->exprType) << "</font></td></tr>";
	out << "<tr><td port='array'>array</td><td port='subscript'>[]</td></tr>";
	out << "</table>>];" << std::endl;
	expr->arr->accept(this);
	out << idx << ":array -> " << lastNode << ";" << std::endl;
	expr->subscript->accept(this);
	out << idx << ":subscript -> " << lastNode << ";" << std::endl;
	lastNode = idx;
}

void ASTVisualizer::visit(ASTExprMemberAccess *expr)
{
	size_t idx = cntNode++;
	out << idx << " [shape=none,label=<";
	out << "<table border='0' cellborder='1' cellspacing='0' bgcolor='" << exprColor(expr) << "'>";
	out << "<tr><td colspan='3'>Member expr</td></tr>";
	out << "<tr><td colspan='3'><font face='Consolas'>" << type2html(expr->exprType) << "</font></td></tr>";
	out << "<tr><td port='object'>object</td>";
	out << "<td><font face='Consolas'>.</font></td>";
	out << "<td><font face='Consolas'>" << symbol.vSymbol[expr->memberName] << "</font></td></tr>";
	out << "</table>>];" << std::endl;
	expr->object->accept(this);
	out << idx << ":object -> " << lastNode << ";" << std::endl;
	lastNode = idx;
}

void ASTVisualizer::visit(ASTExprFuncCall *expr)
{
	size_t idx = cntNode++;
	out << idx << " [shape=none,label=<";
	out << "<table border='0' cellborder='1' cellspacing='0' bgcolor='" << exprColor(expr) << "'>";
	out << "<tr><td colspan='4'>Function Call</td></tr>";
	out << "<tr><td colspan='4'><font face='Consolas'>" << type2html(expr->exprType) << "</font></td></tr>";
	out << "<tr><td colspan='4'>func id: " << expr->funcID << "</td></tr>";
	out << "<tr><td port='func'>func</td>";
	out << "<td><font face='Consolas'>(</font></td>";
	out << "<td port='param'>param</td>";
	out << "<td><font face='Consolas'>)</font></td></tr>";
	out << "</table>>];" << std::endl;
	expr->func->accept(this);
	out << idx << ":func -> " << lastNode << ";" << std::endl;
	for (auto &param : expr->paramList)
	{
		param->accept(this);
		out << idx << ":param -> " << lastNode << ";" << std::endl;
	}
	lastNode = idx;
}

void ASTVisualizer::visit(ASTStatementReturn *stat)
{
	size_t idx = cntNode++;
	out << idx << " [shape=none,label=<";
	out << "<table border='0' cellborder='1' cellspacing='0'>";
	out << "<tr><td><font face='Consolas' color='#569cd6'>return</font></td>";
	if (stat->retExpr)
		out << "<td port='expr'>expr</td>";
	out << "</tr>";
	out << "</table>>];" << std::endl;
	if (stat->retExpr)
	{
		stat->retExpr->accept(this);
		out << idx << ":expr -> " << lastNode << ";" << std::endl;
	}
	lastNode = idx;
}

void ASTVisualizer::visit(ASTStatementBreak *stat)
{
	size_t idx = cntNode++;
	out << idx << " [shape=none,label=<";
	out << "<table border='0' cellborder='1' cellspacing='0'>";
	out << "<tr><td><font face='Consolas' color='#569cd6'>break</font></td></tr>";
	out << "</table>>];" << std::endl;
	lastNode = idx;
}

void ASTVisualizer::visit(ASTStatementContinue *stat)
{
	size_t idx = cntNode++;
	out << idx << " [shape=none,label=<";
	out << "<table border='0' cellborder='1' cellspacing='0'>";
	out << "<tr><td><font face='Consolas' color='#569cd6'>continue</font></td></tr>";
	out << "</table>>];" << std::endl;
	lastNode = idx;
}

void ASTVisualizer::visit(ASTStatementIf *stat)
{
	size_t idx = cntNode++;
	out << idx << " [shape=none,label=<";
	out << "<table border='0' cellborder='1' cellspacing='0'>";
	size_t colspan = stat->elseStat ? 7 : 5;
	out << "<tr><td colspan='" << colspan << "'>if statement</td></tr>";
	out << "<tr><td><font face='Consolas' color='#569cd6'>if</font></td>";
	out << "<td><font face='Consolas'>(</font></td>";
	out << "<td port='cond'>cond</td>";
	out << "<td><font face='Consolas'>)</font></td>";
	out << "<td port='true'>true</td>";
	if (stat->elseStat)
	{
		out << "<td><font face='Consolas' color='#569cd6'>else</font></td>";
		out << "<td port='false'>false</td>";
	}
	out << "</tr>";
	out << "</table>>];" << std::endl;
	stat->exprCond->accept(this);
	out << idx << ":cond -> " << lastNode << ";" << std::endl;
	stat->ifStat->accept(this);
	out << idx << ":true -> " << lastNode << ";" << std::endl;
	if (stat->elseStat)
	{
		stat->elseStat->accept(this);
		out << idx << ":false -> " << lastNode << ";" << std::endl;
	}
	lastNode = idx;
}

void ASTVisualizer::visit(ASTStatementWhile *stat)
{
	size_t idx = cntNode++;
	out << idx << " [shape=none,label=<";
	out << "<table border='0' cellborder='1' cellspacing='0'>";
	out << "<tr><td colspan='5'>while statement</td></tr>";
	out << "<tr><td><font face='Consolas' color='#569cd6'>while</font></td>";
	out << "<td><font face='Consolas'>(</font></td>";
	out << "<td port='cond'>cond</td>";
	out << "<td><font face='Consolas'>)</font></td>";
	out << "<td port='body'>body</td>";
	out << "</tr>";
	out << "</table>>];" << std::endl;
	stat->exprCond->accept(this);
	out << idx << ":cond -> " << lastNode << ";" << std::endl;
	for (auto &st : stat->stats)
	{
		st->accept(this);
		out << idx << ":body -> " << lastNode << ";" << std::endl;
	}
	lastNode = idx;
}

void ASTVisualizer::visit(ASTStatementFor *stat)
{
	size_t idx = cntNode++;
	out << idx << " [shape=none,label=<";
	out << "<table border='0' cellborder='1' cellspacing='0'>";
	out << "<tr><td colspan='9'>for statement</td></tr>";
	out << "<tr><td><font face='Consolas' color='#569cd6'>for</font></td>";
	out << "<td><font face='Consolas'>(</font></td>";
	out << "<td port='init'>init</td>";
	out << "<td><font face='Consolas'>;</font></td>";
	out << "<td port='cond'>cond</td>";
	out << "<td><font face='Consolas'>;</font></td>";
	out << "<td port='step'>step</td>";
	out << "<td><font face='Consolas'>)</font></td>";
	out << "<td port='body'>body</td></tr>";
	out << "</table>>];" << std::endl;
	if (!stat->varDecls.empty())
	{
		for (auto &declVar : stat->varDecls)
		{
			declVar->accept(this);
			out << idx << ":init -> " << lastNode << ";" << std::endl;
		}
	}
	else
	{
		if (stat->exprIn)
		{
			stat->exprIn->accept(this);
			out << idx << ":init -> " << lastNode << ";" << std::endl;
		}
	}
	if (stat->exprCond)
	{
		stat->exprCond->accept(this);
		out << idx << ":cond -> " << lastNode << ";" << std::endl;
	}
	if (stat->exprStep)
	{
		stat->exprStep->accept(this);
		out << idx << ":step -> " << lastNode << ";" << std::endl;
	}
	for (auto &st : stat->stats)
	{
		st->accept(this);
		out << idx << ":body -> " << lastNode << ";" << std::endl;
	}
	lastNode = idx;
}

void ASTVisualizer::visit(ASTStatementBlock *stat)
{
	size_t idx = cntNode++;
	out << idx << " [shape=box,label=\"block\"];" << std::endl;
	for (auto &st : stat->stats)
	{
		st->accept(this);
		out << idx << " -> " << lastNode << ";" << std::endl;
	}
	lastNode = idx;
}

void ASTVisualizer::visit(ASTStatementExpr *stat)
{
	size_t idx = cntNode++;
	out << idx << " [shape=box,label=\"expr stat\"];" << std::endl;
	stat->expr->accept(this);
	out << idx << " -> " << lastNode << ";" << std::endl;
	lastNode = idx;
}

std::string ASTVisualizer::exprColor(ASTExpr *expr)
{
	if (expr->vType == lvalue)
		return "#feffe5";
	else
		return "#e5fcff";
}