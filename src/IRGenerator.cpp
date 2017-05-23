#include "common_headers.h"
#include "IRGenerator.h"
using namespace MxAST;
using namespace MxIR;

Operand IRGenerator::RegByType(size_t regid, MxType type)
{
	if (type.isObject())
		return RegPtr(regid);
	if (type.mainType == MxType::Bool)
		return Reg8(regid);
	if (type.mainType == MxType::Integer)
		return Reg32(regid);
	if (type.mainType == MxType::Void)
		return EmptyOperand();
	assert(false);
	return EmptyOperand();
}

Operand IRGenerator::RegByType(size_t regid, Operand other)
{
	switch (other.type)
	{
	case Operand::imm8: case Operand::reg8:
		return Reg8(regid);
	case Operand::imm16: case Operand::reg16:
		return Reg16(regid);
	case Operand::imm32: case Operand::reg32:
		return Reg32(regid);
	case Operand::imm64: case Operand::reg64:
		return Reg64(regid);
	case Operand::externalSymbolName: case Operand::funcID: case Operand::globalVarID: case Operand::constID:
		return RegPtr(regid);
	default:
		assert(false);
	}
	return EmptyOperand();
}

Operand IRGenerator::ImmByType(std::int64_t imm, MxType type)
{
	if (type.isObject())
		return ImmPtr(imm);
	if (type.mainType == MxType::Bool)
		return Imm8(imm);
	if (type.mainType == MxType::Integer)
		return Imm32(imm);
	assert(false);
	return EmptyOperand();
}

Operand IRGenerator::ImmByType(std::int64_t imm, Operand other)
{
	switch (other.type)
	{
	case Operand::imm8: case Operand::reg8:
		return Imm8(imm);
	case Operand::imm16: case Operand::reg16:
		return Imm16(imm);
	case Operand::imm32: case Operand::reg32:
		return Imm32(imm);
	case Operand::imm64: case Operand::reg64:
		return Imm64(imm);
	case Operand::externalSymbolName: case Operand::funcID: case Operand::globalVarID: case Operand::constID:
		return ImmPtr(imm);
	default:
		assert(false);
	}
	return EmptyOperand();
}

void IRGenerator::merge(std::shared_ptr<Block> &currentBlock, std::shared_ptr<Block> &blkIn, std::shared_ptr<Block> &blkOut)
{
	assert(!currentBlock->brTrue && !currentBlock->brFalse);
	currentBlock->ins.splice(currentBlock->ins.end(), std::move(blkIn->ins));
	currentBlock->brTrue = blkIn->brTrue;
	currentBlock->brFalse = blkIn->brFalse;
	if(blkIn != blkOut)
		currentBlock = blkOut;
	blkIn.reset();
	blkOut.reset();
}

void IRGenerator::merge(std::shared_ptr<Block> &currentBlock)
{
	assert(!currentBlock->brTrue && !currentBlock->brFalse);
	if (lastBlockIn)
	{
		assert(lastBlockOut);
		merge(currentBlock, lastBlockIn, lastBlockOut);
	}
	else
	{
		currentBlock->ins.splice(currentBlock->ins.end(), std::move(lastIns));
	}
}

Instruction IRGenerator::releaseXValue(Operand addr, MxType type)
{
	assert(type.isObject());
	if (addr.isImm() && addr.val == 0)
		return IRNop();
	if (addr.type == Operand::constID)		//constant string
		return IRNop();
	if (type.mainType == MxType::String)
	{
		if (type.arrayDim == 0)
			return IRCall(EmptyOperand(), IDFunc(size_t(MxBuiltin::BuiltinFunc::release_string)), { addr });
		else
			return IRCall(EmptyOperand(), IDFunc(size_t(MxBuiltin::BuiltinFunc::release_array_string)), {addr});
	}
	if (type.mainType == MxType::Bool || type.mainType == MxType::Integer)
	{
		assert(type.arrayDim > 0);
		return IRCall(EmptyOperand(), IDFunc(size_t(MxBuiltin::BuiltinFunc::release_array_internal)), { addr, Imm32(type.arrayDim) });
	}
	assert(type.mainType == MxType::Object);
	if (type.arrayDim > 0)
		return IRCall(EmptyOperand(), IDFunc(size_t(MxBuiltin::BuiltinFunc::release_array_object)), { addr, Imm32(type.arrayDim) });
	return IRCall(EmptyOperand(), IDFunc(size_t(MxBuiltin::BuiltinFunc::release_object)), { addr });
}

Function IRGenerator::generate(ASTDeclFunc *declFunc)
{
	Function ret;
	const MxProgram::funcInfo &finfo = program->vFuncs[declFunc->funcID];
	regNum = program->vLocalVars[declFunc->funcID].size();
	if (finfo.isThiscall)
	{
		regThis = regNum++;
		ret.params.push_back(RegPtr(regThis));	//this
	}
	for (size_t i = 0; i < finfo.paramType.size(); i++)
	{
		if (i == 0 && finfo.isThiscall)
			continue;
		ret.params.push_back(RegByType(i - 1, finfo.paramType[i]));
	}

	ret.inBlock = Block::construct();
	returnBlock = Block::construct();
	std::shared_ptr<Block> currentBlock = ret.inBlock;
	for (auto &stat : declFunc->stats)
	{
		stat->accept(this);
		merge(currentBlock);
		if (currentBlock->brTrue || currentBlock->brFalse)
			break;
	}

	if (currentBlock->ins.empty() || currentBlock->ins.back().oper != Operation::Return)
	{
		currentBlock->ins.push_back(IRReturn());
		currentBlock->brTrue = returnBlock;
	}
	ret.outBlock = std::move(returnBlock);

	//redirectReturn(ret.inBlock, ret.outBlock);
	return ret;
}

void IRGenerator::redirectReturn(std::shared_ptr<Block> inBlock, std::shared_ptr<Block> outBlock)
{
	inBlock->traverse([&outBlock](Block *block)
	{
		if (block == outBlock.get())
			return true;
		if (block->ins.back().oper == Operation::Return)
			block->brTrue = outBlock;
		return true;
	});
}

void IRGenerator::visit(ASTDeclVar *declVar)
{
	if (declVar->initVal)
	{
		std::shared_ptr<Block> blk(Block::construct());
		std::shared_ptr<Block> cur = blk;
		setFlag(Read);
		//declVar->initVal->accept(this);
		visitExprRec(declVar->initVal.get());
		resumeFlag();
		merge(cur);

		//std::list<Instruction> &insList = lastBlockOut ? lastBlockOut->ins : lastIns;
		cur->ins.push_back(IR(RegByType(declVar->varID, declVar->varType), Move, lastOperand));

		ASTExpr *initExpr = dynamic_cast<ASTExpr *>(declVar->initVal.get());
		assert(initExpr);
		if (initExpr->exprType.isObject())
			cur->ins.push_back(IRCall(EmptyOperand(), IDFunc(size_t(MxBuiltin::BuiltinFunc::addref_object)), { lastOperand }));

		clearXValueStack();
		merge(cur);

		lastBlockIn = std::move(blk);
		lastBlockOut = std::move(cur);
	}
	else
		lastIns = { IR(RegByType(declVar->varID, declVar->varType), Move, ImmByType(0, declVar->varType)) };
}

void IRGenerator::visit(ASTExprImm *imm)
{
	assert(!(visFlag & Write));
	if (!(visFlag & Read))
		return;
	
	if (imm->exprType.mainType == MxType::Bool)
		lastOperand = Imm8(imm->exprVal.bvalue ? 1 : 0);
	else if (imm->exprType.mainType == MxType::Integer)
		lastOperand = Imm32(imm->exprVal.ivalue);
	else if (imm->exprType.mainType == MxType::String)
		lastOperand = IDConst(mapStringConstID.find(imm->exprVal.strId)->second);
	else
	{
		assert(imm->exprType.isNull());
		lastOperand = ImmPtr(0);
	}
}

void IRGenerator::visit(ASTExprVar *var)
{
	if (!visFlag)
		return;

	assert(var->exprType.mainType != MxType::Function);
	if (var->isThis)
	{
		assert(!(visFlag & Write));
		lastOperand = RegPtr(regThis);
	}
	else if (var->isGlobalVar)
	{
		lastWriteAddr = IDGlobalVar(var->varID);
		if (visFlag & Read)
		{
			lastIns = { IR(RegByType(regNum++, var->exprType), Load, lastWriteAddr) };
			lastOperand = RegByType(regNum - 1, var->exprType);
		}
	}
	else
	{
		lastOperand = RegByType(var->varID, var->exprType);
		lastWriteAddr = EmptyOperand();
	}
}

void IRGenerator::visit(ASTExprUnary *unary)
{
	if (!visFlag && !unary->sideEffect)
		return;

	ASTExpr *operand = dynamic_cast<ASTExpr *>(unary->operand.get());
	assert(operand);
	MxType operandType = operand->exprType;

	if (unary->oper == ASTExprUnary::IncPostfix || unary->oper == ASTExprUnary::DecPostfix)
	{
		assert(!(visFlag & Write));
		setFlag(Read | Write);
		//unary->operand->accept(this);
		visitExprRec(unary->operand.get());
		resumeFlag();

		std::list<Instruction> &insList = lastBlockOut ? lastBlockOut->ins : lastIns;
		Operand retval;
		if (visFlag & Read)
		{
			retval = RegByType(regNum++, unary->exprType);
			insList.push_back(IR(retval, Move, lastOperand));
		}

		if (lastWriteAddr.type != Operand::empty)
		{
			Operand regtmp = RegByType(regNum++, unary->exprType);
			insList.push_back(IR(regtmp, unary->oper == ASTExprUnary::IncPostfix ? Add : Sub, lastOperand, ImmByType(1, operandType)));
			insList.push_back(IRStore(regtmp, lastWriteAddr));
		}
		else
			insList.push_back(IR(lastOperand, unary->oper == ASTExprUnary::IncPostfix ? Add : Sub, lastOperand, ImmByType(1, operandType)));

		if(visFlag & Read)
			lastOperand = retval;
		return;
	}
	if (unary->oper == ASTExprUnary::Increment || unary->oper == ASTExprUnary::Decrement)
	{
		setFlag(Read | Write);
		//unary->operand->accept(this);
		visitExprRec(unary->operand.get());
		resumeFlag();

		std::list<Instruction> &insList = lastBlockOut ? lastBlockOut->ins : lastIns;
		if (lastWriteAddr.type != Operand::empty)
		{
			Operand regtmp = RegByType(regNum++, unary->exprType);
			insList.push_back(IR(regtmp, unary->oper == ASTExprUnary::Increment ? Add : Sub, lastOperand, ImmByType(1, operandType)));
			insList.push_back(IRStore(regtmp, lastWriteAddr));
			lastOperand = regtmp;
		}
		else
			insList.push_back(IR(lastOperand, unary->oper == ASTExprUnary::Increment ? Add : Sub, lastOperand, ImmByType(1, operandType)));
		return;
	}
	assert(!(visFlag & Write));
	//unary->operand->accept(this);
	visitExprRec(unary->operand.get());

	if (!(visFlag & Read))
		return;

	std::list<Instruction> &insList = lastBlockOut ? lastBlockOut->ins : lastIns;
	switch (unary->oper)
	{
	case ASTExprUnary::Positive:
		break;
	case ASTExprUnary::Negative:
		insList.push_back(IR(RegByType(regNum++, unary->exprType), Neg, lastOperand));
		lastOperand = RegByType(regNum - 1, unary->exprType);
		break;
	case ASTExprUnary::Not:
		insList.push_back(IR(Reg8(regNum++), Seq, lastOperand, ImmByType(0, operandType)));
		lastOperand = Reg8(regNum - 1);
		break;
	case ASTExprUnary::BitNot:
		insList.push_back(IR(RegByType(regNum++, unary->exprType), Not, lastOperand));
		lastOperand = RegByType(regNum - 1, unary->exprType);
		break;
	default:
		assert(false);
	}
}

void IRGenerator::visit(ASTExprBinary *binary)
{
	assert(!(visFlag & Write));

	std::shared_ptr<Block> blkIn(Block::construct());
	std::shared_ptr<Block> cur = blkIn;
	if (!visFlag)
	{
		if (binary->sideEffect)
		{
			binary->operandL->accept(this);
			merge(cur);
			binary->operandR->accept(this);
			merge(cur);
			lastBlockIn = std::move(blkIn);
			lastBlockOut = std::move(cur);
		}
		lastOperand = lastWriteAddr = EmptyOperand();
		return;
	}

	ASTExpr *operandL = dynamic_cast<ASTExpr *>(binary->operandL.get());
	ASTExpr *operandR = dynamic_cast<ASTExpr *>(binary->operandR.get());
	assert(operandL && operandR);

	if (binary->oper == ASTExprBinary::And || binary->oper == ASTExprBinary::Or)
	{
		std::shared_ptr<Block> blkNext(Block::construct()), blkSkip(Block::construct()), blkFinal(Block::construct());

		setFlag(Read);
		//binary->operandL->accept(this);
		visitExprRec(binary->operandL.get());
		resumeFlag();

		merge(cur);
		//Operand cond = Reg8(regNum++);
		//cur->ins.push_back(IR(cond, Sne, lastOperand, ImmByType(0, operandL->exprType)));
		Operand retVal;
		if (visFlag & Read)
			retVal = Reg8(regNum++);
		cur->ins.push_back(IRBranch(lastOperand));

		if (binary->oper == ASTExprBinary::And)
		{
			cur->brTrue = blkNext;
			cur->brFalse = (visFlag & Read) ? blkSkip : blkFinal;
			blkSkip->ins = {
				IR(retVal, Move, Imm8(0)),
				IRJump(),
			};
			blkSkip->brTrue = blkFinal;
		}
		else
		{
			cur->brTrue = (visFlag & Read) ? blkSkip : blkFinal;
			cur->brFalse = blkNext;
			blkSkip->ins = {
				IR(retVal, Move, Imm8(1)),
				IRJump(),
			};
			blkSkip->brTrue = blkFinal;
		}

		cur = blkNext;
		//binary->operandR->accept(this);
		visitExprRec(binary->operandR.get());
		merge(cur);

		cur->ins.push_back(IR(retVal, Move, lastOperand));
		cur->ins.push_back(IRJump());
		cur->brTrue = blkFinal;

		if (visFlag & Read)
			lastOperand = retVal;
		else
			lastOperand = EmptyOperand();
		lastBlockIn = std::move(blkIn);
		lastBlockOut = std::move(blkFinal);
		lastWriteAddr = EmptyOperand();
		return;
	}

	typedef ASTExprBinary::Operator Op;
	static const std::unordered_map<int, Operation> mapOper = {
		{Op::Plus, Add}, {Op::Minus, Sub}, {Op::Multiple, Mult}, {Op::Divide, Div}, {Op::Mod, Mod},
		{Op::ShiftLeft, Shl}, {Op::ShiftRight, Shr}, {Op::BitAnd, And}, {Op::BitXor, Xor}, {Op::BitOr, Or},
		{Op::Equal, Seq}, {Op::NotEqual, Sne}, {Op::GreaterThan, Sgt}, {Op::GreaterEqual, Sge}, {Op::LessThan, Slt}, {Op::LessEqual, Sle}
	};

	if (operandL->exprType.mainType == MxType::String)
	{
		assert(operandR->exprType.mainType == MxType::String);
		if (binary->oper == Op::Plus)
		{
			generateFuncCall(size_t(MxBuiltin::BuiltinFunc::strcat), { operandL, operandR });
			merge(cur);
		}
		else
		{
			generateFuncCall(size_t(MxBuiltin::BuiltinFunc::strcmp), { operandL, operandR });
			merge(cur);
			if (visFlag & Read)
			{
				Operand result = lastOperand;
				lastOperand = Reg8(regNum++);
				cur->ins.push_back(IR(lastOperand, mapOper.find(binary->oper)->second, result, Imm32(0)));
			}
		}
		lastWriteAddr = EmptyOperand();
		lastBlockIn = std::move(blkIn);
		lastBlockOut = std::move(cur);
		return;
	}

	Operand resultL, resultR;
	//binary->operandL->accept(this);
	visitExprRec(binary->operandL.get());
	resultL = lastOperand;
	merge(cur);
	
	//binary->operandR->accept(this);
	visitExprRec(binary->operandR.get());
	resultR = lastOperand;
	merge(cur);

	if (visFlag & Read)
	{
		cur->ins.push_back(IR(RegByType(regNum++, binary->exprType), mapOper.find(binary->oper)->second, resultL, resultR));
		lastOperand = RegByType(regNum - 1, binary->exprType);
	}
	else
		lastOperand = EmptyOperand();
	lastBlockIn = std::move(blkIn);
	lastBlockOut = std::move(cur);
	lastWriteAddr = EmptyOperand();
}

void IRGenerator::visit(ASTExprAssignment *assign)
{
	std::shared_ptr<Block> blkIn(Block::construct());
	std::shared_ptr<Block> cur = blkIn;

	setFlag(Read);
	//assign->operandR->accept(this);
	visitExprRec(assign->operandR.get());
	resumeFlag();
	Operand assignValue = lastOperand;
	merge(cur);

	setFlag(Write);
	//assign->operandL->accept(this);
	visitExprRec(assign->operandL.get());
	resumeFlag();
	merge(cur);

	if (lastWriteAddr.type != Operand::empty)
	{
		cur->ins.push_back(IRStore(assignValue, lastWriteAddr));
		lastOperand = assignValue;
	}
	else
		cur->ins.push_back(IR(lastOperand, Move, assignValue));

	if (assign->exprType.isObject())
		cur->ins.push_back(IRCall(EmptyOperand(), IDFunc(size_t(MxBuiltin::BuiltinFunc::addref_object)), { assignValue }));

	lastBlockIn = std::move(blkIn);
	lastBlockOut = std::move(cur);
}

void IRGenerator::visit(ASTExprNew *exprNew)
{
	std::shared_ptr<Block> blkIn(Block::construct());
	std::shared_ptr<Block> cur = blkIn;
	std::vector<Operand> param;

	setFlag(Read);
	for (auto &p : exprNew->paramList)
	{
		//p->accept(this);
		if (!p)
			break;
		visitExprRec(p.get());
		merge(cur);
		param.push_back(lastOperand);
	}
	resumeFlag();

	typedef std::tuple<std::shared_ptr<Block>, std::shared_ptr<Block>, Operand> rettype;

	auto genNewObject = [this](size_t objSize, Operand typeID, size_t funcID, const std::vector<Operand> &params) -> rettype
	{
		std::shared_ptr<Block> blk(Block::construct());

		Operand addrObj = RegPtr(regNum++);
		blk->ins = { 
			IRCall(addrObj, IDFunc(size_t(MxBuiltin::BuiltinFunc::newobject_zero)), { ImmPtr(objSize), typeID })
		};
		if (funcID != size_t(-1))
		{
			std::vector<Operand> cstParam = { addrObj };
			cstParam.insert(cstParam.end(), params.begin(), params.end());
			blk->ins.push_back(IRCall(EmptyOperand(), IDFunc(funcID), cstParam));	//FIXME: parameters
		}

		return std::make_tuple(blk, blk, addrObj);
	};

	auto genNew1DArray = [exprNew, this]
	(Operand elementNum, size_t elementSize, Operand typeID, std::function<rettype()> funcInit) -> rettype	//generate 1-dim array
	{
		std::shared_ptr<Block> blkIn(Block::construct());
		Operand tmpElementNum = RegPtr(regNum++);
		Operand tmpSize = RegPtr(regNum++);
		Operand addrArray = RegPtr(regNum++);
		blkIn->ins = {
			IR(tmpElementNum, Sext, elementNum),
			IR(tmpSize, Mult, tmpElementNum, ImmPtr(elementSize)),
			IR(tmpSize, Add, tmpSize, ImmPtr(8)),
			IRCall(addrArray,
				IDFunc(size_t(funcInit ? MxBuiltin::BuiltinFunc::newobject : MxBuiltin::BuiltinFunc::newobject_zero)), {tmpSize, typeID}),
			IRStore(tmpElementNum, addrArray),
		};
		if (funcInit)
		{
			std::shared_ptr<Block> blkCond(Block::construct()), blkFinal(Block::construct());
			Operand tmpOffset = RegPtr(regNum++);
			blkIn->ins.push_back(IR(tmpOffset, Move, ImmPtr(8)));
			blkIn->ins.push_back(IRJump());
			blkIn->brTrue = blkCond;

			Operand tmpCmp = Reg8(regNum++);
			blkCond->ins = {
				IR(tmpCmp, Sltu, tmpOffset, tmpSize),
				IRBranch(tmpCmp, likely),
			};
			std::shared_ptr<Block> blkBodyIn, blkBodyOut;
			Operand elementContent;
			std::tie(blkBodyIn, blkBodyOut, elementContent) = funcInit();
			blkCond->brTrue = blkBodyIn;
			blkCond->brFalse = blkFinal;
			
			blkBodyOut->ins.push_back(IRStoreA(elementContent, addrArray, tmpOffset));
			blkBodyOut->ins.push_back(IR(tmpOffset, Add, tmpOffset, ImmPtr(elementSize)));
			blkBodyOut->ins.push_back(IRJump());
			blkBodyOut->brTrue = blkCond;

			return std::make_tuple(blkIn, blkFinal, addrArray);
		}
		return std::make_tuple(blkIn, blkIn, addrArray);
	};
	std::function<rettype(size_t)> genNewArray;
	genNewArray = [&](size_t i) -> rettype
	{
		ASTExpr *paramExpr = dynamic_cast<ASTExpr *>(exprNew->paramList[i].get());
		assert(paramExpr);
		if (i < param.size() - 1)
			return genNew1DArray(param[i], POINTER_SIZE, ImmPtr(0),
				[&genNewArray, i]() { return genNewArray(i + 1); });
		else if (i < exprNew->paramList.size() - 1)
			return genNew1DArray(param[i], POINTER_SIZE, ImmPtr(0), std::function<rettype()>());
		if (exprNew->exprType.mainType == MxType::Object)
		{
			return genNew1DArray(param[i], POINTER_SIZE, ImmPtr(0), 
				[&]() { return genNewObject(program->vClass[exprNew->exprType.className].classSize, ImmPtr(0), exprNew->funcID, {}); });
		}
		return genNew1DArray(param[i], MxType{ exprNew->exprType.mainType }.getSize(), ImmPtr(0), std::function<rettype()>());	//TODO: type id
	};

	if (exprNew->isArray)
	{
		std::shared_ptr<Block> tmpIn, tmpOut;
		std::tie(tmpIn, tmpOut, lastOperand) = genNewArray(0);
		merge(cur, tmpIn, tmpOut);
	}
	else
	{
		std::shared_ptr<Block> tmpIn, tmpOut;
		std::tie(tmpIn, tmpOut, lastOperand) = genNewObject(program->vClass[exprNew->exprType.className].classSize, ImmPtr(0), exprNew->funcID, param);
		merge(cur, tmpIn, tmpOut);
	}

	lastBlockIn = std::move(blkIn);
	lastBlockOut = std::move(cur);
	lastWriteAddr = EmptyOperand();
}

void IRGenerator::visit(ASTExprSubscriptAccess *exprSub)
{
	std::shared_ptr<Block> blkIn(Block::construct());
	std::shared_ptr<Block> cur = blkIn;
	if (!visFlag)
	{
		if (exprSub->sideEffect)
		{
			//exprSub->arr->accept(this);
			visitExprRec(exprSub->arr.get());
			merge(cur);
			//exprSub->subscript->accept(this);
			visitExprRec(exprSub->subscript.get());
			merge(cur);

			lastBlockIn = blkIn;
			lastBlockOut = cur;
		}
		lastOperand = lastWriteAddr = EmptyOperand();
		return;
	}

	setFlag(Read);
	//exprSub->arr->accept(this);
	visitExprRec(exprSub->arr.get());
	Operand arr = lastOperand;
	merge(cur);
	//exprSub->subscript->accept(this);
	visitExprRec(exprSub->subscript.get());
	Operand subscript = lastOperand;
	merge(cur);
	resumeFlag();

	lastWriteAddr = RegPtr(regNum++);
	if(exprSub->exprType.isObject())
		cur->ins.push_back(IRCall(lastWriteAddr, IDFunc(size_t(MxBuiltin::BuiltinFunc::subscript_object)), { arr, subscript }));
	else if (exprSub->exprType.mainType == MxType::Bool)
		cur->ins.push_back(IRCall(lastWriteAddr, IDFunc(size_t(MxBuiltin::BuiltinFunc::subscript_bool)), { arr, subscript }));
	else if (exprSub->exprType.mainType == MxType::Integer)
		cur->ins.push_back(IRCall(lastWriteAddr, IDFunc(size_t(MxBuiltin::BuiltinFunc::subscript_int)), { arr, subscript }));
	else
	{
		assert(false);
	}
	
	if (visFlag & Read)
	{
		lastOperand = RegByType(regNum++, exprSub->exprType);
		cur->ins.push_back(IR(lastOperand, Load, lastWriteAddr));
	}
	else
		lastOperand = EmptyOperand();
	ASTExpr *exprArr = dynamic_cast<ASTExpr *>(exprSub->arr.get());

	lastBlockIn = std::move(blkIn);
	lastBlockOut = std::move(cur);
}

void IRGenerator::visit(ASTExprMemberAccess *expr)
{
	if (!visFlag)
	{
		if (expr->sideEffect)
			visitExprRec(expr->object.get());
			//expr->object->accept(this);
		return;
	}

	ASTExpr *obj = dynamic_cast<ASTExpr *>(expr->object.get());
	assert(obj);

	setFlag(Read);
	visitExprRec(expr->object.get());
	resumeFlag();
	if (expr->exprType.mainType == MxType::Function)	//return object pointer
	{
		assert(!(visFlag & Write));
		return;
	}

	Operand addrObj = lastOperand;
	std::list<Instruction> &insList = lastBlockOut ? lastBlockOut->ins : lastIns;

	lastWriteAddr = RegPtr(regNum++);
	insList.push_back(IR(lastWriteAddr, Add, 
		addrObj, ImmPtr(program->vClass[obj->exprType.className].members[expr->memberID].offset)));

	if (visFlag & Read)
	{
		lastOperand = RegByType(regNum++, expr->exprType);
		insList.push_back(IR(lastOperand, Load, lastWriteAddr));
	}
	else
		lastOperand = EmptyOperand();
}

void IRGenerator::visit(ASTExprFuncCall *expr)
{
	std::vector<ASTExpr *> param;
	if (program->vFuncs[expr->funcID].isThiscall)
	{
		ASTExprMemberAccess *memAccess = dynamic_cast<ASTExprMemberAccess *>(expr->func.get());
		assert(memAccess);
		param.push_back(memAccess);
	}
	for (auto &p : expr->paramList)
	{
		ASTExpr *expr = dynamic_cast<ASTExpr *>(p.get());
		assert(expr);
		param.push_back(expr);
	}
	generateFuncCall(expr->funcID, param);
}

void IRGenerator::visit(ASTStatementReturn *stat)
{
	std::shared_ptr<Block> blk(Block::construct());
	std::shared_ptr<Block> cur = blk;
	if (stat->retExpr)
	{
		ASTExpr *expr = dynamic_cast<ASTExpr *>(stat->retExpr.get());
		assert(expr);
		setFlag(Read);
		//stat->retExpr->accept(this);
		visitExpr(stat->retExpr.get());
		resumeFlag();
		merge(cur);
		
		if (expr->exprType.isObject())
			cur->ins.push_back(IRCall(EmptyOperand(), IDFunc(size_t(MxBuiltin::BuiltinFunc::addref_object)), { lastOperand }));
		cur->ins.push_back(IRReturn(lastOperand));
	}
	else
		cur->ins = { IRReturn() };
	cur->brTrue = returnBlock;
	lastBlockIn = std::move(blk);
	lastBlockOut = std::move(cur);
}

void IRGenerator::visit(ASTStatementBreak *stat)
{
	std::shared_ptr<Block> blk(Block::construct());
	blk->ins = { IRJump() };
	blk->brTrue = loopBreak;
	lastBlockIn = blk;
	lastBlockOut = std::move(blk);
}

void IRGenerator::visit(ASTStatementContinue *stat)
{
	std::shared_ptr<Block> blk(Block::construct());
	blk->ins = { IRJump() };
	blk->brTrue = loopContinue;
	lastBlockIn = blk;
	lastBlockOut = std::move(blk);
}

void IRGenerator::visit(ASTStatementIf *stat)
{
	std::shared_ptr<Block> blkIn(Block::construct()), blkTrue(Block::construct()), blkFalse(Block::construct()), blkFinal(Block::construct());
	std::shared_ptr<Block> cur = blkIn;

	setFlag(Read);
	visitExpr(stat->exprCond.get());
	Operand cond = lastOperand;
	merge(cur);
	resumeFlag();

	cur->ins.push_back(IRBranch(cond));
	std::shared_ptr<Block> blkCond = cur;

	if (stat->ifStat)
	{
		blkCond->brTrue = blkTrue;
		cur = blkTrue;
		stat->ifStat->accept(this);
		merge(cur);
		if (!cur->brTrue && !cur->brFalse)
		{
			cur->ins.push_back(IRJump());
			cur->brTrue = blkFinal;
		}
	}
	else
		blkCond->brTrue = blkFinal;

	if (stat->elseStat)
	{
		blkCond->brFalse = blkFalse;
		cur = blkFalse;
		stat->elseStat->accept(this);
		merge(cur);
		if (!cur->brTrue && !cur->brFalse)
		{
			cur->ins.push_back(IRJump());
			cur->brTrue = blkFinal;
		}
	}
	else
		blkCond->brFalse = blkFinal;

	lastBlockIn = std::move(blkIn);
	lastBlockOut = std::move(blkFinal);
}

void IRGenerator::visit(ASTStatementWhile *stat)
{
	std::shared_ptr<Block> blkIn(Block::construct()), blkCond(Block::construct()), blkBody(Block::construct()), blkFinal(Block::construct());
	blkIn->ins = { IRJump() };
	blkIn->brTrue = blkCond;
	std::shared_ptr<Block> cur = blkCond;

	setFlag(Read);
	visitExpr(stat->exprCond.get());
	Operand cond = lastOperand;
	merge(cur);
	resumeFlag();

	cur->ins.push_back(IRBranch(cond));
	cur->brTrue = blkBody;
	cur->brFalse = blkFinal;
	std::shared_ptr<Block> oldBreak = std::move(loopBreak);
	std::shared_ptr<Block> oldContinue = std::move(loopContinue);
	loopBreak = blkFinal;
	loopContinue = blkCond;

	cur = blkBody;
	visit(static_cast<ASTBlock *>(stat));
	merge(cur);

	if (!cur->brTrue && !cur->brFalse)
	{
		cur->ins.push_back(IRJump());
		cur->brTrue = blkCond;
	}

	lastBlockIn = std::move(blkIn);
	lastBlockOut = std::move(blkFinal);
	loopBreak = std::move(oldBreak);
	loopContinue = std::move(oldContinue);
}

void IRGenerator::visit(ASTStatementFor *stat)
{
	std::shared_ptr<Block> blkIn(Block::construct()), blkCond(Block::construct()), blkBody(Block::construct()), blkStep(Block::construct()), blkFinal(Block::construct());
	std::shared_ptr<Block> cur = blkIn;

	if (stat->exprIn)
	{
		stat->exprIn->accept(this);
		merge(cur);
	}
	for (auto &var : stat->varDecls)
	{
		var->accept(this);
		merge(cur);
	}
	cur->ins.push_back(IRJump());
	cur->brTrue = blkCond;

	cur = blkCond;
	if (stat->exprCond)
	{
		setFlag(Read);
		visitExpr(stat->exprCond.get());
		merge(cur);
		Operand cond = lastOperand;
		resumeFlag();

		cur->ins.push_back(IRBranch(cond));
		cur->brTrue = blkBody;
		cur->brFalse = blkFinal;
	}
	else
	{
		cur->ins.push_back(IRJump());
		cur->brTrue = blkBody;
	}

	cur = blkBody;
	std::shared_ptr<Block> oldBreak = std::move(loopBreak);
	std::shared_ptr<Block> oldContinue = std::move(loopContinue);
	loopBreak = blkFinal;
	loopContinue = blkStep;
	visit(static_cast<ASTBlock *>(stat));
	merge(cur);
	if (!cur->brTrue && !cur->brFalse)
	{
		cur->ins.push_back(IRJump());
		cur->brTrue = blkStep;
	}

	cur = blkStep;
	if (stat->exprStep)
	{
		stat->exprStep->accept(this);
		merge(cur);
	}
	cur->ins.push_back(IRJump());
	cur->brTrue = blkCond;

	lastBlockIn = std::move(blkIn);
	lastBlockOut = std::move(blkFinal);
	loopBreak = std::move(oldBreak);
	loopContinue = std::move(oldContinue);
}

void IRGenerator::visit(ASTStatementExpr *stat)
{
	setFlag(0);
	visitExpr(stat->expr.get());
	resumeFlag();
}

void IRGenerator::visit(ASTBlock *block)
{
	std::shared_ptr<Block> blk(Block::construct());
	std::shared_ptr<Block> cur = blk;

	for (auto &stat : block->stats)
	{
		stat->accept(this);
		merge(cur);
		if (cur->brTrue || cur->brFalse)
			break;
	}

	lastBlockIn = std::move(blk);
	lastBlockOut = std::move(cur);
}

void IRGenerator::generateFuncCall(size_t funcID, const std::vector<ASTExpr *> &param)
{
	assert(!(visFlag & Write));
	std::shared_ptr<Block> blk(Block::construct());
	std::shared_ptr<Block> cur = blk;
	const MxProgram::funcInfo &finfo = program->vFuncs[funcID];
	if (!visFlag && (finfo.attribute & NoSideEffect))
	{
		for (auto &p : param)
		{
			visitExprRec(p);
			merge(cur);
		}
		lastBlockIn = std::move(blk);
		lastBlockOut = std::move(cur);
		return;
	}
	
	setFlag(Read);
	std::vector<Operand> pValue;
	for (auto &p : param)
	{
		visitExprRec(p);
		merge(cur);
		pValue.push_back(lastOperand);
	}
	resumeFlag();

	lastOperand = RegByType(regNum++, finfo.retType);
	cur->ins.push_back(IRCall(lastOperand, IDFunc(funcID), pValue));

	lastBlockIn = std::move(blk);
	lastBlockOut = std::move(cur);
}

void IRGenerator::visitExprRec(ASTNode *node)
{
	lastWriteAddr = lastOperand = EmptyOperand();
	ASTExpr *expr = dynamic_cast<ASTExpr *>(node);
	assert(expr);
	if (!visFlag && !expr->sideEffect)
		return;

	if ((visFlag & Write) && expr->exprType.isObject())
	{
		setFlag(visFlag | Read);
		expr->accept(this);
		resumeFlag();
		stkXValues.push({ lastOperand, expr->exprType });
	}
	else if (expr->vType == xvalue)
	{
		expr->accept(this);
		if (lastOperand.type != Operand::empty)
			stkXValues.push({ lastOperand, expr->exprType });
	}
	else
		expr->accept(this);
}

void IRGenerator::visitExpr(ASTNode *node)
{
	std::shared_ptr<Block> blk(Block::construct());
	std::shared_ptr<Block> cur = blk;
	visitExprRec(node);
	merge(cur);
	clearXValueStack();
	merge(cur);
	
	lastBlockIn = std::move(blk);
	lastBlockOut = std::move(cur);
}

void IRGenerator::clearXValueStack()
{
	while (!stkXValues.empty())
	{
		auto xvalue = stkXValues.top();
		stkXValues.pop();
		lastIns.push_back(releaseXValue(xvalue.first, xvalue.second));
	}
}


size_t IRGenerator::findMain()
{
	size_t funcMain = size_t(-1);
	for (size_t i = 0; i < program->vFuncs.size(); i++)
	{
		const MxProgram::funcInfo &finfo = program->vFuncs[i];
		if (symbol->vSymbol[finfo.funcName] == "main")
		{
			if (funcMain != size_t(-1))
			{
				issues->error(0, -1, "conflicting declaration of function main");
				return size_t(-1);
			}
			if (!finfo.paramType.empty())
			{
				issues->error(0, -1, "main function must have no parameter");
				return size_t(-1);
			}
			if (finfo.retType.mainType != MxType::Integer)
			{
				issues->error(0, -1, "main function must return int");
				return size_t(-1);
			}
			funcMain = i;
		}
	}
	if (funcMain == size_t(-1))
		issues->error(0, -1, "main function not found");
	return funcMain;
}

void IRGenerator::generateProgram(MxAST::ASTRoot *root)
{
	size_t funcMain = findMain();
	if (funcMain == size_t(-1))
		return;

	for (auto &clsInfo : program->vClass)
	{
		std::int64_t curOffset = 0;
		for (auto &mem : clsInfo.second.members)
		{
			mem.offset = alignAddr(curOffset, mem.varType.getSize());
			curOffset = mem.offset + mem.varType.getSize();
		}
		clsInfo.second.classSize = curOffset;
	}

	assert(symbol->vString.size() == symbol->vStringRefCount.size());
	for (size_t i = 0; i < symbol->vString.size(); i++)
	{
		if (symbol->vStringRefCount[i] == 0)
			continue;
		mapStringConstID[i] = program->vConst.size();
		program->vConst.push_back(MxBuiltin::string2Const(symbol->vString[i]));
	}

	std::shared_ptr<Block> blkIn(Block::construct());
	std::shared_ptr<Block> cur = blkIn;

	regNum = 0;

	for (auto &child : root->nodes)
	{
		ASTDeclVarGlobal *declVar = dynamic_cast<ASTDeclVarGlobal *>(child.get());
		if (declVar)
		{
			if (declVar->initVal)
			{
				setFlag(Read);
				visitExprRec(declVar->initVal.get());
				resumeFlag();
				merge(cur);
				cur->ins.push_back(IRStore(lastOperand, IDGlobalVar(declVar->varID)));

				ASTExpr *initExpr = dynamic_cast<ASTExpr *>(declVar->initVal.get());
				assert(initExpr);
				if (initExpr->exprType.isObject())
					cur->ins.push_back(IRCall(EmptyOperand(), IDFunc(size_t(MxBuiltin::BuiltinFunc::addref_object)), { lastOperand }));

				clearXValueStack();
				merge(cur);
			}
		}
	}

	cur->ins.push_back(IRReturn());

	Function funcStart;
	funcStart.inBlock = std::move(blkIn);
	funcStart.outBlock = Block::construct();
	redirectReturn(funcStart.inBlock, funcStart.outBlock);

	program->vFuncs[size_t(MxBuiltin::BuiltinFunc::initialize)].content = std::move(funcStart);

	regNum = 0;

	for (auto &child : root->nodes)
	{
		ASTDeclFunc *declFunc = dynamic_cast<ASTDeclFunc *>(child.get());
		if (declFunc)
		{
			program->vFuncs[declFunc->funcID].content = generate(declFunc);
			continue;
		}
		ASTDeclClass *declClass = dynamic_cast<ASTDeclClass *>(child.get());
		if (declClass)
		{
			for (auto &member : declClass->vMemFuncs)
			{
				ASTDeclFunc *memberFunc = dynamic_cast<ASTDeclFunc *>(member.get());
				assert(memberFunc);
				program->vFuncs[memberFunc->funcID].content = generate(memberFunc);
			}
		}
	}

	program->vFuncs[funcMain].content.inBlock->ins.push_front(IRCall(EmptyOperand(), IDFunc(size_t(MxBuiltin::BuiltinFunc::initialize)), {}));
	program->vFuncs[funcMain].attribute |= Export;
}