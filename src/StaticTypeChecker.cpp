#include "common_headers.h"
#include "StaticTypeChecker.h"
#include "MxBuiltin.h"
using namespace MxAST;

StaticTypeChecker::StaticTypeChecker(MxProgram *memTable, GlobalSymbol *symbols, IssueCollector *issues) :
	program(memTable), symbols(symbols), issues(issues), curClass(-1), curFunc(-1), depthLoop(0)
{
	for (size_t i = 0; i < memTable->vGlobalVars.size(); i++)
		mapGlobalVar.insert({ memTable->vGlobalVars[i].varName, i });
	for (auto &cls : memTable->vClass)
	{
		mapClassMemberId.insert({ cls.first, std::map<size_t, size_t>() });
		for (size_t i = 0; i < cls.second.members.size(); i++)
			mapClassMemberId[cls.first].insert({ cls.second.members[i].varName, i });
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
		program->vClass.insert({ declClass->className, {0, std::vector<MxProgram::varInfo>() } });
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
			std::vector<MxProgram::varInfo> &varTable = program->vClass[declClass->className].members;
			for (auto &memFunc : declClass->vMemFuncs)
			{
				ASTDeclFunc *declFunc = dynamic_cast<ASTDeclFunc *>(memFunc.get());
				assert(declFunc);

				if (!checkFunc(declFunc, mapVarId, varTable, declClass->className))
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
				issues->error(declFunc->tokenL, declFunc->tokenR,
					"class and global variable using the same name is not allowed");
				flag = false;
			}
			///////////////////////////////////////////////////
			if (!checkFunc(declFunc, mapGlobalVar, program->vGlobalVars, size_t(-1)))
				flag = false;
		}
	}
	program->vLocalVars.resize(program->vFuncs.size());
	return flag;
}

bool StaticTypeChecker::checkFunc(MxAST::ASTDeclFunc *declFunc, 
	std::map<size_t, size_t> &mapVarId, std::vector<MxProgram::varInfo> &varTable,
	size_t className)
{
	if (!checkType(declFunc->retType, declFunc->tokenL, declFunc->tokenR))
		return false;

	std::vector<MxType> thisParamType;
	if (className != size_t(-1))
		thisParamType.push_back(MxType{ MxType::Object, 0, className });
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
		MxProgram::varInfo vinfo = varTable[iter->second];
		assert(vinfo.varType.mainType == MxType::Function);		//we always consider functions first
		size_t olid = vinfo.varType.funcOLID;
		for (size_t funcId : program->vOverloadedFuncs[olid])
		{
			std::vector<MxType> paramType = program->vFuncs[funcId].paramType;
			if (std::equal(paramType.begin(), paramType.end(), thisParamType.begin(), thisParamType.end()))
			{
				issues->error(declFunc->tokenL, declFunc->tokenR,
					"Redefinition of function '" + symbols->vSymbol[declFunc->funcName] + "'");
				return false;
			}
		}
		declFunc->funcID = program->vFuncs.size();
		program->vOverloadedFuncs[olid].push_back(declFunc->funcID);	//overload +1
	}
	else
	{
		declFunc->funcID = program->vFuncs.size();
		size_t olid = program->vOverloadedFuncs.size();
		program->vOverloadedFuncs.push_back({ declFunc->funcID });		//overload +1
		mapVarId.insert({ declFunc->funcName, varTable.size() });
		varTable.push_back(MxProgram::varInfo{ declFunc->funcName, MxType{MxType::Function, 0, size_t(-1), olid} });	//var +1
	}
	program->vFuncs.push_back(MxProgram::funcInfo{ 
		declFunc->funcName, declFunc->retType, thisParamType, 0, className != size_t(-1) });	//func +1
	return true;
}

bool StaticTypeChecker::checkVar(MxAST::ASTDeclVar *declVar, std::map<size_t, size_t> &mapVarId, std::vector<MxProgram::varInfo> &varTable)
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
	varTable.push_back(MxProgram::varInfo{ declVar->varName, declVar->varType });
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

size_t StaticTypeChecker::findConstructor(size_t className, const std::vector<MxType> &vTypes)
{
	auto iterClass = mapClassMemberId.find(className);
	assert(iterClass != mapClassMemberId.end());
	auto iterFunc = iterClass->second.find(className);
	if (iterFunc != iterClass->second.end())
	{
		const MxProgram::varInfo &vInfo = program->vClass[className].members[iterFunc->second];
		if (vInfo.varType.mainType == MxType::Function)
		{
			std::vector<MxType> params = { MxType{MxType::Object, 0, className} };
			std::copy(vTypes.begin(), vTypes.end(), std::back_inserter(params));
			return findOverloadedFunc(vInfo.varType.funcOLID, params);
		}
	}
	return size_t(-1);
}

size_t StaticTypeChecker::findOverloadedFunc(size_t olid, const std::vector<MxType> &vTypes)
{
	size_t matched = size_t(-1);
	for (size_t funcID : program->vOverloadedFuncs[olid])
	{
		const auto &params = program->vFuncs[funcID].paramType;
		if (params.size() != vTypes.size())
			continue;

		bool flag = true;
		for (size_t i = 0; i < params.size(); i++)
			if (params[i] != vTypes[i])
			{
				flag = false;
				break;
			}
		if (flag)
		{
			if (matched != size_t(-1))
				return size_t(-2);
			matched = funcID;
		}
	}
	return matched;
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
	program->vLocalVars[declFunc->funcID] = std::move(vLocalVar);
	vLocalVar.clear();
	mapLocalVar.clear();
	return declFunc;
}

ASTNode * StaticTypeChecker::enter(MxAST::ASTDeclVarGlobal *declVar)
{
	//// SAME class name & var name is not allowed ////
	if(curClass == -1 && mapClassMemberId.find(declVar->varName) != mapClassMemberId.end())
	{
		issues->error(declVar->tokenL, declVar->tokenR,
			"class and global variable using the same name is not allowed");
	}
	///////////////////////////////////////////////////
	if(curClass == -1)
		checkVar(declVar, mapGlobalVar, program->vGlobalVars);
	return declVar;
}

ASTNode * StaticTypeChecker::enter(MxAST::ASTDeclVarLocal *declVar)
{
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
	vLocalVar.push_back(MxProgram::varInfo{ declVar->varName, declVar->varType });
	return declVar;
}

ASTNode * StaticTypeChecker::leave(MxAST::ASTDeclVar *declVar)
{
	ASTExpr *initVal = dynamic_cast<ASTExpr *>(declVar->initVal.get());
	if (initVal)
	{
		assert(initVal);
		if (declVar->varType != initVal->exprType)
			issues->error(declVar->tokenL, declVar->tokenR,
				"type mismatch when initialize variable");
	}
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
		var->vType = rvalue;
		var->exprType = MxType{ MxType::Object, 0, curClass, size_t(-1) };
		return var;
	}
	auto iterLocal = mapLocalVar.find(var->varName);
	if (iterLocal != mapLocalVar.end())
	{
		var->isGlobalVar = false;
		var->varID = iterLocal->second.top();
		var->exprType = vLocalVar[var->varID].varType;
		var->vType = var->exprType.mainType == MxType::Function ? rvalue : lvalue;
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
			tmpThis->vType = lvalue;
			std::unique_ptr<ASTExprMemberAccess> ret(new ASTExprMemberAccess);
			ret->tokenL = var->tokenL;
			ret->tokenR = var->tokenR;
			ret->memberName = var->varName;
			ret->memberID = iterMember->second;
			ret->object = std::move(tmpThis);
			ret->exprType = program->vClass[curClass].members[ret->memberID].varType;
			ret->vType = ret->exprType.mainType == MxType::Function ? rvalue : lvalue;
			return ret.release();
		}
	}
	auto iterGlobal = mapGlobalVar.find(var->varName);
	if (iterGlobal != mapGlobalVar.end())
	{
		var->isGlobalVar = true;
		var->varID = iterGlobal->second;
		var->exprType = program->vGlobalVars[var->varID].varType;
		var->vType = var->exprType.mainType == MxType::Function ? rvalue : lvalue;
		return var;
	}
	issues->error(var->tokenL, var->tokenR,
		"'" + symbols->vSymbol[var->varName] + "' is not declared in this scope");
	return var;
}

ASTNode * StaticTypeChecker::leave(MxAST::ASTExprImm *imm)
{
	imm->vType = rvalue;
	return imm;
}

ASTNode * StaticTypeChecker::leave(MxAST::ASTExprUnary *expr)
{
	ASTExpr *operand = dynamic_cast<ASTExpr *>(expr->operand.get());
	assert(operand);
	if (expr->oper == ASTExprUnary::Not)
	{
		if (operand->exprType != MxType{ MxType::Bool })
			issues->error(expr->tokenL, expr->tokenR,
				"bool type required");
		expr->exprType = MxType{ MxType::Bool };
	}
	else
	{
		if (operand->exprType != MxType{ MxType::Integer })
			issues->error(expr->tokenL, expr->tokenR,
				"integer type required");
		expr->exprType = MxType{ MxType::Integer };
	}
	if (expr->oper == ASTExprUnary::Increment || expr->oper == ASTExprUnary::Decrement ||
		expr->oper == ASTExprUnary::IncPostfix || expr->oper == ASTExprUnary::DecPostfix)
	{
		if (operand->vType != lvalue)
			issues->error(expr->tokenL, expr->tokenR,
				"lvalue required");
	}
	if (expr->oper == ASTExprUnary::Increment || expr->oper == ASTExprUnary::Decrement)
		expr->vType = lvalue;
	else
		expr->vType = rvalue;
	return expr;
}

ASTNode * StaticTypeChecker::leave(MxAST::ASTExprBinary *expr)
{
	ASTExpr *operandL = dynamic_cast<ASTExpr *>(expr->operandL.get());
	ASTExpr *operandR = dynamic_cast<ASTExpr *>(expr->operandR.get());
	expr->vType = rvalue;
	switch (expr->oper)
	{
	case ASTExprBinary::Plus: 
		if (operandL->exprType == MxType{ MxType::Integer } && operandR->exprType == MxType{ MxType::Integer })
			expr->exprType = MxType{ MxType::Integer };
		else if (operandL->exprType == MxType{ MxType::String } && operandR->exprType == MxType{ MxType::String })
		{
			expr->exprType = MxType{ MxType::String };
			expr->vType = xvalue;
		}
		else
			issues->error(expr->tokenL, expr->tokenR,
				"Both operands need to be integer or string");
		break;

	case ASTExprBinary::Minus: case ASTExprBinary::Multiple: case ASTExprBinary::Divide:
	case ASTExprBinary::Mod: case ASTExprBinary::ShiftLeft: case ASTExprBinary::ShiftRight:
	case ASTExprBinary::BitAnd: case ASTExprBinary::BitXor: case ASTExprBinary::BitOr:
		if (operandL->exprType != MxType{ MxType::Integer } || operandR->exprType != MxType{ MxType::Integer })
			issues->error(expr->tokenL, expr->tokenR,
				"Both operands need to be integer");
		expr->exprType = MxType{ MxType::Integer };
		break;

	case ASTExprBinary::And: case ASTExprBinary::Or:
		if (operandL->exprType != MxType{ MxType::Bool } || operandR->exprType != MxType{ MxType::Bool })
			issues->error(expr->tokenL, expr->tokenR,
				"Both operands need to be bool");
		expr->exprType = MxType{ MxType::Bool };
		break;

	case ASTExprBinary::Equal: case ASTExprBinary::NotEqual:
		expr->exprType = MxType{ MxType::Bool };
		if (operandL->exprType == MxType{ MxType::Void } || operandR->exprType == MxType{ MxType::Void })
			issues->error(expr->tokenL, expr->tokenR,
				"Cannot compare void type");
		if (operandL->exprType != operandR->exprType)
			issues->error(expr->tokenL, expr->tokenR,
				"Operands need to be the same type");
		break;

	case ASTExprBinary::LessEqual: case ASTExprBinary::LessThan:
	case ASTExprBinary::GreaterEqual: case ASTExprBinary::GreaterThan:
		expr->exprType = MxType{ MxType::Bool };
		if (!((operandL->exprType == MxType{ MxType::Integer } && operandR->exprType == MxType{ MxType::Integer }) ||
			(operandL->exprType == MxType{ MxType::String } && operandR->exprType == MxType{ MxType::String })))
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
	if (operandL->vType != lvalue)
		issues->error(expr->tokenL, expr->tokenR,
			"lvalue required as left operand of assignment");
	if (operandR->exprType == MxType{ MxType::Void })
		issues->error(expr->tokenL, expr->tokenR,
			"void type is not ignored as it ought to be");
	if (operandL->exprType != operandR->exprType)
		issues->error(expr->tokenL, expr->tokenR,
			"Both operands need to be the same type");
	expr->vType = lvalue;
	expr->exprType = operandL->exprType;
	return expr;
}

ASTNode * StaticTypeChecker::leave(MxAST::ASTExprNew *expr)
{
	if (!expr->isArray && expr->exprType.mainType != MxType::Object)
	{
		issues->error(expr->tokenL, expr->tokenR,
			"new expression of internal type is not supported");
		return expr;
	}
	if (!expr->isArray && !expr->paramList.empty())
	{
		std::vector<MxType> paramType;
		for (auto &p : expr->paramList)
		{
			ASTExpr *expr = dynamic_cast<ASTExpr *>(p.get());
			assert(expr);
			paramType.push_back(expr->exprType);
		}
		size_t funcID = findConstructor(expr->exprType.className, paramType);
		if (funcID == size_t(-2))
		{
			issues->error(expr->tokenL, expr->tokenR,
				"ambiguous call to constuctor of " + symbols->vSymbol[expr->exprType.className]);
			return expr;
		}
		if (funcID == size_t(-1))
		{
			issues->error(expr->tokenL, expr->tokenR,
				"no matched constructor");
			return expr;
		}
		expr->funcID = funcID;
		return expr;
	}
	expr->vType = xvalue;
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
	if (expr->exprType.mainType == MxType::Object)
	{
		size_t funcID = findConstructor(expr->exprType.className, {});
		assert(funcID != size_t(-2));
		expr->funcID = funcID;
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
	expr->vType = lvalue;
	return expr;
}

ASTNode * StaticTypeChecker::leave(MxAST::ASTExprMemberAccess *expr)
{
	ASTExpr *object = dynamic_cast<ASTExpr *>(expr->object.get());
	assert(object);
	size_t className = MxBuiltin::getBuiltinClassByType(object->exprType);
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
	expr->exprType = program->vClass[className].members[expr->memberID].varType;
	expr->vType = expr->exprType.mainType == MxType::Function ? rvalue : lvalue;
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
	expr->vType = rvalue;
	size_t olid = func->exprType.funcOLID;

	//TODO: use findOverloadedFunc instead
	bool matched = false;
	for (size_t funcID : program->vOverloadedFuncs[olid])
	{
		const MxProgram::funcInfo &finfo = program->vFuncs[funcID];
		std::vector<MxType>::const_iterator iterParam;
		if (program->vFuncs[funcID].isThiscall)
		{
			if (finfo.paramType.size() != expr->paramList.size() + 1)
				continue;
			iterParam = finfo.paramType.begin() + 1;
		}
		else
		{
			if (finfo.paramType.size() != expr->paramList.size())
				continue;
			iterParam = finfo.paramType.begin();
		}

		bool flag = true;
		for (auto iterCall = expr->paramList.begin(); iterCall != expr->paramList.end(); ++iterCall, ++iterParam)
		{
			ASTExpr *paramExpr = dynamic_cast<ASTExpr *>(iterCall->get());
			assert(paramExpr);
			if (paramExpr->exprType != *iterParam)
			{
				flag = false;
				break;
			}
		}
		if (!flag)
			continue;
		if (matched)
		{
			issues->error(expr->tokenL, expr->tokenR,
				"ambiguous function call");
			continue;
		}
		matched = true;
		expr->funcID = funcID;
		expr->exprType = finfo.retType;
		if (finfo.retType.mainType == MxType::String || finfo.retType.mainType == MxType::Object || finfo.retType.arrayDim > 0)
			expr->vType = xvalue;
		if(curFunc != -1)
			program->vFuncs[curFunc].dependency.insert(funcID);
	}
	if(!matched)
		issues->error(expr->tokenL, expr->tokenR,
			"No such function");
	return expr;
}

ASTNode * StaticTypeChecker::leave(MxAST::ASTStatementReturn *stat)
{
	MxType retType = program->vFuncs[curFunc].retType;
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