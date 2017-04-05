#include "StaticTypeChecker.h"
#include "MxBuiltin.h"
using namespace MxAST;

StaticTypeChecker::StaticTypeChecker(MemberTable *memTable, GlobalSymbol *symbols, IssueCollector *issues) :
	memTable(memTable), symbols(symbols), issues(issues), curClass(-1), curFunc(-1), depthLoop(0)
{
	fillBuiltinMemberTable(memTable);
	for (size_t i = 0; i < memTable->vGlobalVars.size(); i++)
		mapGlobalVar.insert({ memTable->vGlobalVars[i].varName, i });
	for (auto &cls : memTable->vClassMembers)
	{
		mapClassMemberId.insert({ cls.first, std::map<size_t, size_t>() });
		for (size_t i = 0; i < cls.second.size(); i++)
			mapClassMemberId[cls.first].insert({ cls.second[i].varName, i });
	}
}

bool StaticTypeChecker::preCheck(ASTRoot *root)
{
	bool flag = true;
	for (auto &classNode : root->nodes)
	{
		ASTDeclClass *declClass = dynamic_cast<ASTDeclClass *>(classNode.get());
		if (!declClass)
			continue;
		if (mapClassMemberId.find(declClass->className) != mapClassMemberId.end())
		{
			issues->error(declClass->tokenL, declClass->tokenR,
				"Redefinition of class '" + symbols->vSymbol[declClass->className] + "'");
			flag = false;
			continue;
		}
		mapClassMemberId.insert({ declClass->className, std::map<size_t, size_t>() });
		memTable->vClassMembers.insert({ declClass->className, std::vector<MemberTable::varInfo>() });
	}
	if (!flag)
		return false;
	flag = true;

	for (auto &classNode : root->nodes)
	{
		ASTDeclClass *declClass = dynamic_cast<ASTDeclClass *>(classNode.get());
		if (declClass)
		{
			std::map<size_t, size_t> &mapVarId = mapClassMemberId[declClass->className];	//var name -> var id
			std::vector<MemberTable::varInfo> &varTable = memTable->vClassMembers[declClass->className];
			for (auto &memFunc : declClass->vMemFuncs)
			{
				ASTDeclFunc *declFunc = dynamic_cast<ASTDeclFunc *>(memFunc.get());
				assert(declFunc);

				//// SAME class name & var name is not allowed ////
				if (declFunc->funcName != declClass->className && 
					mapClassMemberId.find(declFunc->funcName) != mapClassMemberId.end())
				{
					issues->error(declClass->tokenL, declClass->tokenR,
						"class and global variable using the same name is not allowed");
					flag = false;
				}
				///////////////////////////////////////////////////

				if (!checkFunc(declFunc, mapVarId, varTable))
					flag = false;
			}
			for (auto &memVar : declClass->vMemVars)
			{
				ASTDeclVar *declVar = dynamic_cast<ASTDeclVar *>(memVar.get());
				assert(declVar);
				if (!checkVar(declVar, mapVarId, varTable))
					flag = false;
			}
			continue;
		}
		ASTDeclFunc *declFunc = dynamic_cast<ASTDeclFunc *>(classNode.get());
		if (declFunc)
		{
			//// SAME class name & var name is not allowed ////
			if (mapClassMemberId.find(declFunc->funcName) != mapClassMemberId.end())
			{
				issues->error(declClass->tokenL, declClass->tokenR,
					"class and global variable using the same name is not allowed");
				flag = false;
			}
			///////////////////////////////////////////////////
			if (!checkFunc(declFunc, mapGlobalVar, memTable->vGlobalVars))
				flag = false;
		}
	}
	memTable->vLocalVars.resize(memTable->vFuncs.size());
	return flag;
}

bool StaticTypeChecker::checkFunc(MxAST::ASTDeclFunc *declFunc, std::map<size_t, size_t> &mapVarId, std::vector<MemberTable::varInfo> &varTable)
{
	if (!checkType(declFunc->retType, declFunc->tokenL, declFunc->tokenR))
		return false;

	std::vector<MxType> thisParamType;
	for (auto &node : declFunc->param)
	{
		ASTDeclVar *dVar = dynamic_cast<ASTDeclVar *>(node.get());
		assert(dVar);
		if (!checkType(dVar->varType, dVar->tokenL, dVar->tokenR))
			return false;
		thisParamType.push_back(dVar->varType);
	}

	auto iter = mapVarId.find(declFunc->funcName);
	if (iter != mapVarId.end())
	{
		MemberTable::varInfo vinfo = varTable[iter->second];
		assert(vinfo.varType.mainType == MxType::Function);		//we always consider functions first
		size_t olid = vinfo.varType.funcOLID;
		for (size_t funcId : memTable->vOverloadedFuncs[olid])
		{
			std::vector<MxType> paramType = memTable->vFuncs[funcId].paramType;
			if (std::equal(paramType.begin(), paramType.end(), thisParamType.begin(), thisParamType.end()))
			{
				issues->error(declFunc->tokenL, declFunc->tokenR,
					"Redefinition of function '" + symbols->vSymbol[declFunc->funcName] + "'");
				return false;
			}
		}
		declFunc->funcID = memTable->vFuncs.size();
		memTable->vOverloadedFuncs[olid].push_back(declFunc->funcID);	//overload +1
	}
	else
	{
		declFunc->funcID = memTable->vFuncs.size();
		size_t olid = memTable->vOverloadedFuncs.size();
		memTable->vOverloadedFuncs.push_back({ declFunc->funcID });		//overload +1
		mapVarId.insert({ declFunc->funcName, varTable.size() });
		varTable.push_back(MemberTable::varInfo{ declFunc->funcName, MxType{MxType::Function, 0, size_t(-1), olid} });	//var +1
	}
	memTable->vFuncs.push_back(MemberTable::funcInfo{ declFunc->funcName, declFunc->retType, thisParamType });	//func +1
	return true;
}

bool StaticTypeChecker::checkVar(MxAST::ASTDeclVar *declVar, std::map<size_t, size_t> &mapVarId, std::vector<MemberTable::varInfo> &varTable)
{
	if (!checkType(declVar->varType, declVar->tokenL, declVar->tokenR))
		return false;

	auto iter = mapVarId.find(declVar->varName);
	if (iter != mapVarId.end())
	{
		issues->error(declVar->tokenL, declVar->tokenR,
			"Redefinition of '" + symbols->vSymbol[declVar->varName] + "'");
		return false;
	}
	declVar->varID = varTable.size();
	mapVarId.insert({ declVar->varName, declVar->varID });
	varTable.push_back(MemberTable::varInfo{ declVar->varName, declVar->varType });
	return true;
}

bool StaticTypeChecker::checkType(MxType type, ssize_t tokenL, ssize_t tokenR)
{
	if (type.mainType == MxType::Object)
	{
		if (mapClassMemberId.find(type.className) == mapClassMemberId.end())
		{
			issues->error(tokenL, tokenR,
				"unrecognized symbol '" + symbols->vSymbol[type.className] + "'");
			return false;
		}
	}
	return true;
}

ASTNode * StaticTypeChecker::enter(MxAST::ASTDeclClass *declClass)
{
	curClass = declClass->className;
	return declClass;
}

ASTNode * StaticTypeChecker::leave(MxAST::ASTDeclClass *declClass)
{
	curClass = -1;
	return declClass;
}

ASTNode * StaticTypeChecker::enter(MxAST::ASTDeclFunc *declFunc)
{
	curFunc = declFunc->funcID;
	assert(stkCurrentBlockVar.empty());
	enter(static_cast<ASTBlock *>(declFunc));
	return declFunc;
}

ASTNode * StaticTypeChecker::leave(MxAST::ASTDeclFunc *declFunc)
{
	leave(static_cast<ASTBlock *>(declFunc));
	assert(stkCurrentBlockVar.empty());
	assert(depthLoop == 0);
	curFunc = -1;
	memTable->vLocalVars[declFunc->funcID] = std::move(vLocalVar);
	vLocalVar.clear();
	mapLocalVar.clear();
	return declFunc;
}

ASTNode * StaticTypeChecker::enter(MxAST::ASTDeclVarGlobal *declVar)
{
	//// SAME class name & var name is not allowed ////
	if (mapClassMemberId.find(declVar->varName) != mapClassMemberId.end())
	{
		issues->error(declVar->tokenL, declVar->tokenR,
			"class and global variable using the same name is not allowed");
	}
	///////////////////////////////////////////////////
	if(curClass == -1)
		checkVar(declVar, mapGlobalVar, memTable->vGlobalVars);
	return declVar;
}

ASTNode * StaticTypeChecker::enter(MxAST::ASTDeclVarLocal *declVar)
{
	//// SAME class name & var name is not allowed ////
	if (mapClassMemberId.find(declVar->varName) != mapClassMemberId.end())
	{
		issues->error(declVar->tokenL, declVar->tokenR,
			"class and global variable using the same name is not allowed");
	}
	///////////////////////////////////////////////////
	checkType(declVar->varType, declVar->tokenL, declVar->tokenR);

	auto iter = mapLocalVar.find(declVar->varName);
	auto &curBlock = stkCurrentBlockVar.top();
	if (iter != mapLocalVar.end())
	{
		size_t oldVarId = iter->second.top();
		if (curBlock.find(oldVarId) != curBlock.end())
		{
			issues->error(declVar->tokenL, declVar->tokenR,
				"Redefinition of local variable '" + symbols->vSymbol[declVar->varName] + "'");
			return declVar;
		}
		declVar->varID = vLocalVar.size();
		iter->second.push(declVar->varID);
	}
	else
	{
		declVar->varID = vLocalVar.size();
		std::stack<size_t> tmp;
		tmp.push(declVar->varID);
		mapLocalVar.insert({ declVar->varName, std::move(tmp) });
	}
	curBlock.insert(declVar->varID);
	vLocalVar.push_back(MemberTable::varInfo{ declVar->varName, declVar->varType });
	return declVar;
}

ASTNode * StaticTypeChecker::leave(MxAST::ASTDeclVar *declVar)
{
	ASTExpr *initVal = dynamic_cast<ASTExpr *>(declVar->initVal.get());
	assert(initVal);
	if (declVar->varType != initVal->exprType)
		issues->error(declVar->tokenL, declVar->tokenR,
			"type mismatch when initialize variable");
	return declVar;
}

ASTNode * StaticTypeChecker::enter(MxAST::ASTBlock *block)
{
	stkCurrentBlockVar.emplace();
	return block;
}

ASTNode * StaticTypeChecker::leave(MxAST::ASTBlock *block)
{
	for (size_t i : stkCurrentBlockVar.top())
	{
		mapLocalVar[vLocalVar[i].varName].pop();
		if (mapLocalVar[vLocalVar[i].varName].empty())
			mapLocalVar.erase(vLocalVar[i].varName);
	}
	stkCurrentBlockVar.pop();
	return block;
}

ASTNode * StaticTypeChecker::enter(MxAST::ASTExprVar *var)
{
	if (var->isThis)
	{
		if (curClass == -1)
		{
			issues->error(var->tokenL, var->tokenR,
				"Invalid use of 'this' in non-member function");
			return var;
		}
		var->isLValue = false;
		var->exprType = MxType{ MxType::Object, 0, curClass, size_t(-1) };
		return var;
	}
	auto iterLocal = mapLocalVar.find(var->varName);
	if (iterLocal != mapLocalVar.end())
	{
		var->isGlobalVar = false;
		var->varID = iterLocal->second.top();
		var->exprType = vLocalVar[var->varID].varType;
		var->isLValue = var->exprType.mainType != MxType::Function;
		return var;
	}
	if (curClass != -1)
	{
		auto &mapCurClass = mapClassMemberId[curClass];
		auto iterMember = mapCurClass.find(var->varName);
		if (iterMember != mapCurClass.end())
		{
			std::unique_ptr<ASTExprVar> tmpThis(new ASTExprVar);
			tmpThis->isThis = true;
			tmpThis->exprType = MxType{ MxType::Object, 0, curClass, size_t(-1) };
			tmpThis->isLValue = false;
			std::unique_ptr<ASTExprMemberAccess> ret(new ASTExprMemberAccess);
			ret->tokenL = var->tokenL;
			ret->tokenR = var->tokenR;
			ret->memberName = var->varName;
			ret->memberID = iterMember->second;
			ret->object = std::move(tmpThis);
			ret->exprType = memTable->vClassMembers[curClass][ret->memberID].varType;
			ret->isLValue = ret->exprType.mainType != MxType::Function;
			return ret.release();
		}
	}
	auto iterGlobal = mapGlobalVar.find(var->varName);
	if (iterGlobal != mapGlobalVar.end())
	{
		var->isGlobalVar = true;
		var->varID = iterGlobal->second;
		var->exprType = memTable->vGlobalVars[var->varID].varType;
		var->isLValue = var->exprType.mainType != MxType::Function;
		return var;
	}
	issues->error(var->tokenL, var->tokenR,
		"'" + symbols->vSymbol[var->varName] + "' is not declared in this scope");
	return var;
}

ASTNode * StaticTypeChecker::leave(MxAST::ASTExprImm *imm)
{
	imm->isLValue = false;
	return imm;
}

ASTNode * StaticTypeChecker::leave(MxAST::ASTExprUnary *expr)
{
	ASTExpr *operand = dynamic_cast<ASTExpr *>(expr->operand.get());
	assert(operand);
	if (expr->oper == ASTExprUnary::Not)
	{
		if (operand->exprType.mainType != MxType::Bool)
			issues->error(expr->tokenL, expr->tokenR,
				"bool type required");
		expr->exprType = MxType{ MxType::Bool };
	}
	else
	{
		if (operand->exprType.mainType != MxType::Integer)
			issues->error(expr->tokenL, expr->tokenR,
				"integer type required");
		expr->exprType = MxType{ MxType::Integer };
	}
	if (expr->oper == ASTExprUnary::Increment || expr->oper == ASTExprUnary::Decrement ||
		expr->oper == ASTExprUnary::IncPostfix || expr->oper == ASTExprUnary::DecPostfix)
	{
		if (!operand->isLValue)
			issues->error(expr->tokenL, expr->tokenR,
				"lvalue required");
	}
	if (expr->oper == ASTExprUnary::Increment || expr->oper == ASTExprUnary::Decrement)
		expr->isLValue = true;
	else
		expr->isLValue = false;
	return expr;
}

ASTNode * StaticTypeChecker::leave(MxAST::ASTExprBinary *expr)
{
	ASTExpr *operandL = dynamic_cast<ASTExpr *>(expr->operandL.get());
	ASTExpr *operandR = dynamic_cast<ASTExpr *>(expr->operandR.get());
	expr->isLValue = false;
	switch (expr->oper)
	{
	case ASTExprBinary::Plus: 
		if (operandL->exprType.mainType == MxType::Integer && operandR->exprType.mainType == MxType::Integer)
			expr->exprType = MxType{ MxType::Integer };
		else if (operandL->exprType.mainType == MxType::String && operandR->exprType.mainType == MxType::String)
			expr->exprType = MxType{ MxType::String };
		else
			issues->error(expr->tokenL, expr->tokenR,
				"Both operands need to be integer or string");
		break;

	case ASTExprBinary::Minus: case ASTExprBinary::Multiple: case ASTExprBinary::Divide:
	case ASTExprBinary::Mod: case ASTExprBinary::ShiftLeft: case ASTExprBinary::ShiftRight:
	case ASTExprBinary::BitAnd: case ASTExprBinary::BitXor: case ASTExprBinary::BitOr:
		if (operandL->exprType.mainType != MxType::Integer || operandR->exprType.mainType != MxType::Integer)
			issues->error(expr->tokenL, expr->tokenR,
				"Both operands need to be integer");
		expr->exprType = MxType{ MxType::Integer };
		break;

	case ASTExprBinary::And: case ASTExprBinary::Or:
		if (operandL->exprType.mainType != MxType::Bool || operandR->exprType.mainType != MxType::Bool)
			issues->error(expr->tokenL, expr->tokenR,
				"Both operands need to be bool");
		expr->exprType = MxType{ MxType::Bool };
		break;

	case ASTExprBinary::Equal: case ASTExprBinary::NotEqual:
		expr->exprType = MxType{ MxType::Bool };
		if (operandL->exprType.mainType == MxType::Void || operandR->exprType.mainType == MxType::Void)
			issues->error(expr->tokenL, expr->tokenR,
				"Cannot compare void type");
		if (operandL->exprType != operandR->exprType)
			issues->error(expr->tokenL, expr->tokenR,
				"Operands need to be the same type");
		break;

	case ASTExprBinary::LessEqual: case ASTExprBinary::LessThan:
	case ASTExprBinary::GreaterEqual: case ASTExprBinary::GreaterThan:
		expr->exprType = MxType{ MxType::Bool };
		if(!((operandL->exprType.mainType == MxType::Integer && operandR->exprType.mainType == MxType::Integer) ||
			(operandL->exprType.mainType == MxType::String && operandR->exprType.mainType == MxType::String)))
			issues->error(expr->tokenL, expr->tokenR,
				"Both operands need to be integer or string");
		break;

	default:
		assert(false);
	}
	return expr;
}

ASTNode * StaticTypeChecker::leave(MxAST::ASTExprAssignment *expr)
{
	ASTExpr *operandL = dynamic_cast<ASTExpr *>(expr->operandL.get());
	ASTExpr *operandR = dynamic_cast<ASTExpr *>(expr->operandR.get());
	if (!operandL->isLValue)
		issues->error(expr->tokenL, expr->tokenR,
			"lvalue required as left operand of assignment");
	if (operandR->exprType.mainType == MxType::Void)
		issues->error(expr->tokenL, expr->tokenR,
			"void type is not ignored as it ought to be");
	if (operandL->exprType != operandR->exprType)
		issues->error(expr->tokenL, expr->tokenR,
			"Both operands need to be the same type");
	expr->isLValue = true;
	expr->exprType = operandL->exprType;
	return expr;
}

ASTNode * StaticTypeChecker::leave(MxAST::ASTExprNew *expr)
{
	if (!expr->isArray && !expr->paramList.empty())
	{
		//FIXME
		issues->fatal(expr->tokenL, expr->tokenR,
			"Constructor with parameters not implemented");
		return expr;
	}
	expr->isLValue = false;
	if (expr->isArray)
	{
		for (auto &param : expr->paramList)
		{
			if (!param)
				break;
			ASTExpr *exprParam = dynamic_cast<ASTExpr *>(param.get());
			assert(exprParam);
			if (exprParam->exprType.mainType != MxType::Integer)
				issues->error(exprParam->tokenL, exprParam->tokenR,
					"expression in new-declarator must be integer");
		}
	}
	return expr;
}

ASTNode * StaticTypeChecker::leave(MxAST::ASTExprSubscriptAccess *expr)
{
	ASTExpr *arr = dynamic_cast<ASTExpr *>(expr->arr.get());
	assert(arr);
	expr->exprType = arr->exprType;
	if (arr->exprType.arrayDim == 0)
		issues->error(expr->tokenL, expr->tokenR,
			"array required");
	expr->exprType.arrayDim--;
	expr->isLValue = true;
	return expr;
}

ASTNode * StaticTypeChecker::leave(MxAST::ASTExprMemberAccess *expr)
{
	ASTExpr *object = dynamic_cast<ASTExpr *>(expr->object.get());
	assert(object);
	size_t className = getBuiltinClassByType(object->exprType);
	if (className == size_t(-1))
	{
		if (object->exprType.mainType == MxType::Object && object->exprType.className != -1)
			className = object->exprType.className;
		else
		{
			issues->error(expr->tokenL, expr->tokenR,
				"class type required");
			return expr;
		}
	}
	auto &mapClass = mapClassMemberId[className];
	auto iterMember = mapClass.find(expr->memberName);
	if (iterMember == mapClass.end())
	{
		issues->error(expr->tokenL, expr->tokenR,
			"member '" + symbols->vSymbol[expr->memberName] + "' not found");
		return expr;
	}
	expr->memberID = iterMember->second;
	expr->exprType = memTable->vClassMembers[className][expr->memberID].varType;
	expr->isLValue = expr->exprType.mainType != MxType::Function;
	return expr;
}

ASTNode * StaticTypeChecker::leave(MxAST::ASTExprFuncCall *expr)
{
	ASTExpr *func = dynamic_cast<ASTExpr *>(expr->func.get());
	assert(func);
	if (func->exprType.mainType != MxType::Function)
	{
		issues->error(expr->tokenL, expr->tokenR,
			"function required");
		return expr;
	}
	expr->isLValue = false;
	size_t olid = func->exprType.funcOLID;
	for (size_t funcID : memTable->vOverloadedFuncs[olid])
	{
		if (memTable->vFuncs[funcID].paramType.size() != expr->paramList.size())
			continue;
		bool flag = true;
		for (size_t i = 0; i < expr->paramList.size(); i++)
		{
			ASTExpr *paramExpr = dynamic_cast<ASTExpr *>(expr->paramList[i].get());
			assert(paramExpr);
			if (memTable->vFuncs[funcID].paramType[i] != paramExpr->exprType)
			{
				flag = false;
				break;
			}
		}
		if (flag)
		{
			expr->funcID = funcID;
			expr->exprType = memTable->vFuncs[funcID].retType;
			return expr;
		}
	}
	issues->error(expr->tokenL, expr->tokenR,
		"No such function");
	return expr;
}

ASTNode * StaticTypeChecker::leave(MxAST::ASTStatementReturn *stat)
{
	MxType retType = memTable->vFuncs[curFunc].retType;
	if (retType.mainType == MxType::Void)
	{
		if (stat->retExpr)
			issues->error(stat->tokenL, stat->tokenR,
				"return-statement with a value, in function returning void");
	}
	else
	{
		if (!stat->retExpr)
			issues->error(stat->tokenL, stat->tokenR,
				"return-statement with no value, in function returning non-void");
		else
		{
			ASTExpr *retExpr = dynamic_cast<ASTExpr *>(stat->retExpr.get());
			assert(retExpr);
			if (retExpr->exprType != retType)
				issues->error(stat->tokenL, stat->tokenR,
					"return type mismatch");
		}
	}
	return stat;
}

ASTNode * StaticTypeChecker::leave(MxAST::ASTStatementBreak *stat)
{
	if (depthLoop == 0)
		issues->error(stat->tokenL, stat->tokenR,
			"break-statement not within loop");
	return stat;
}

ASTNode * StaticTypeChecker::leave(MxAST::ASTStatementContinue *stat)
{
	if (depthLoop == 0)
		issues->error(stat->tokenL, stat->tokenR,
			"continue-statement not within loop");
	return stat;
}

ASTNode * StaticTypeChecker::leave(MxAST::ASTStatementIf *stat)
{
	ASTExpr *exprCond = dynamic_cast<ASTExpr *>(stat->exprCond.get());
	assert(exprCond);
	if (exprCond->exprType.mainType != MxType::Bool)
		issues->error(stat->tokenL, stat->tokenR,
			"bool type condition required");
	leave(static_cast<ASTBlock *>(stat));
	return stat;
}

ASTNode * StaticTypeChecker::enter(MxAST::ASTStatementWhile *stat)
{
	enter(static_cast<ASTBlock *>(stat));
	depthLoop++;
	return stat;
}

ASTNode * StaticTypeChecker::leave(MxAST::ASTStatementWhile *stat)
{
	depthLoop--;
	ASTExpr *exprCond = dynamic_cast<ASTExpr *>(stat->exprCond.get());
	assert(exprCond);
	if (exprCond->exprType.mainType != MxType::Bool)
		issues->error(stat->tokenL, stat->tokenR,
			"bool type condition required");
	leave(static_cast<ASTBlock *>(stat));
	return stat;
}

ASTNode * StaticTypeChecker::enter(MxAST::ASTStatementFor *stat)
{
	enter(static_cast<ASTBlock *>(stat));
	depthLoop++;
	return stat;
}

ASTNode * StaticTypeChecker::leave(MxAST::ASTStatementFor *stat)
{
	depthLoop--;
	if (stat->exprCond)
	{
		ASTExpr *exprCond = dynamic_cast<ASTExpr *>(stat->exprCond.get());
		assert(exprCond);
		if (exprCond->exprType.mainType != MxType::Bool)
			issues->error(stat->tokenL, stat->tokenR,
				"bool type condition required");
	}
	leave(static_cast<ASTBlock *>(stat));
	return stat;
}