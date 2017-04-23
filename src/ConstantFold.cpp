#include "common_headers.h"
#include "ConstantFold.h"
using namespace MxAST;

namespace ASTOptimizer
{
	ASTNode * ConstantFold::leave(ASTExprUnary *expr)
	{
		ASTExprImm *imm = dynamic_cast<ASTExprImm *>(expr->operand.get());
		if (!imm)
			return expr;

		std::unique_ptr<ASTExprImm> newImm;
		if (imm->exprType.mainType == MxType::Bool)
			newImm.reset(new ASTExprImm(expr->evaluate(imm->exprVal.bvalue)));
		else if (imm->exprType.mainType == MxType::Integer)
			newImm.reset(new ASTExprImm(expr->evaluate(imm->exprVal.ivalue)));
		else
		{
			assert(imm->exprType.mainType == MxType::Object && imm->exprType.className == size_t(-1)); // null
			assert(expr->oper == ASTExprUnary::Not);
			newImm.reset(new ASTExprImm(true));
		}
		newImm->tokenL = expr->tokenL;
		newImm->tokenR = expr->tokenR;
		return newImm.release();
	}

	ASTNode * ConstantFold::leave(ASTExprBinary *expr)
	{
		ASTExprImm *operandL = dynamic_cast<ASTExprImm *>(expr->operandL.get());
		ASTExprImm *operandR = dynamic_cast<ASTExprImm *>(expr->operandR.get());
		if (!operandL || !operandR)
			return expr;

		if (expr->oper == ASTExprBinary::Divide || expr->oper == ASTExprBinary::Mod)
		{
			assert(operandL->exprType.mainType == MxType::Integer);
			assert(operandR->exprType.mainType == MxType::Integer);
			if (operandR->exprVal.ivalue == 0)
			{
				issues->error(expr->tokenL, expr->tokenR,
					"Divided by zero");
				return expr;
			}
		}

		std::unique_ptr<ASTExprImm> newImm;
		if (operandL->exprType.mainType == MxType::Integer)
		{
			assert(operandR->exprType.mainType == MxType::Integer);
			newImm.reset(new ASTExprImm(expr->evaluate(operandL->exprVal.ivalue, operandR->exprVal.ivalue)));
		}
		else if (operandL->exprType.mainType == MxType::Bool)
		{
			assert(operandR->exprType.mainType == MxType::Bool);
			newImm.reset(new ASTExprImm(expr->evaluate(operandL->exprVal.bvalue, operandR->exprVal.bvalue)));
		}
		else if (operandL->exprType.isNull())
		{
			assert(operandR->exprType.isNull());
			newImm.reset(new ASTExprImm(true));
		}
		else
		{
			assert(operandL->exprType.mainType == MxType::String);
			assert(operandR->exprType.mainType == MxType::String);
			std::string strL = symbols->vString[operandL->exprVal.strId];
			std::string strR = symbols->vString[operandR->exprVal.strId];
			if (expr->oper == ASTExprBinary::Plus)
			{
				if (symbols->sumStringSize + strL.size() + strR.size() > MAX_STRINGSIZE)
					return expr;
				if (symbols->memoryUsage + strL.size() + strR.size() > MAX_STRINGMEMUSAGE)
					return expr;
				size_t sid = symbols->addString(strL + strR);
				symbols->decStringRef(operandL->exprVal.strId);
				symbols->decStringRef(operandR->exprVal.strId);
				newImm.reset(new ASTExprImm);
				newImm->exprType = MxType{ MxType::String };
				newImm->exprVal.strId = sid;
				IF_DEBUG(newImm->strContent = strL + strR);
			}
			else
				newImm.reset(new ASTExprImm(expr->stringCompare(strL, strR)));
		}
		newImm->tokenL = expr->tokenL;
		newImm->tokenR = expr->tokenR;
		return newImm.release();
	}

}