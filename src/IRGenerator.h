#ifndef MX_COMPILER_IR_GENERATOR_H
#define MX_COMPILER_IR_GENERATOR_H

#include "AST.h"
#include "IR.h"
#include "MxProgram.h"
#include "IssueCollector.h"
#include "MxBuiltin.h"

class IRGenerator : protected MxAST::ASTVisitor
{
public:
	IRGenerator() : program(MxProgram::getDefault()), symbol(GlobalSymbol::getDefault()), issues(IssueCollector::getDefault()) {}
	IRGenerator(MxProgram *program, GlobalSymbol *symbol, IssueCollector *issues) : program(program), symbol(symbol), issues(issues) {}

	MxIR::Function generate(MxAST::ASTDeclFunc *declFunc);

	//generate the ir code of the full program and store it to MxProgram
	//note that it will 
	//  1. fillin the size and offset field of MxProgram::vClass
	//  2. add all string in GlobalSymbol to const list
	//  3. fillin the __initialize function
	//  4. add a function call of __initialize to the very beginning of main function
	//  5. add 'Export' attribute to main function
	void generateProgram(MxAST::ASTRoot *root);	

protected:
	static void redirectReturn(std::shared_ptr<MxIR::Block> inBlock, std::shared_ptr<MxIR::Block> outBlock);	//link all blocks that are ended with return to outBlock
	static MxIR::Operand RegByType(size_t regid, MxType type);
	static MxIR::Operand RegByType(size_t regid, MxIR::Operand other);
	static MxIR::Operand ImmByType(std::int64_t imm, MxType type);
	static MxIR::Operand ImmByType(std::int64_t imm, MxIR::Operand other);
	static void merge(std::shared_ptr<MxIR::Block> &currentBlock, std::shared_ptr<MxIR::Block> &blkIn, std::shared_ptr<MxIR::Block> &blkOut);
	void merge(std::shared_ptr<MxIR::Block> &currentBlock);	//merge last block / lastIns to current block
	static MxIR::Instruction releaseXValue(MxIR::Operand addr, MxType type);

	virtual void visit(MxAST::ASTDeclVar *declVar) override;
	virtual void visit(MxAST::ASTExprImm *imm) override;
	virtual void visit(MxAST::ASTExprVar *var) override;
	virtual void visit(MxAST::ASTExprUnary *unary) override;
	virtual void visit(MxAST::ASTExprBinary *binary) override;
	virtual void visit(MxAST::ASTExprAssignment *assign) override;
	virtual void visit(MxAST::ASTExprNew *exprNew) override;
	virtual void visit(MxAST::ASTExprSubscriptAccess *exprSub) override;
	virtual void visit(MxAST::ASTExprMemberAccess *expr) override;
	virtual void visit(MxAST::ASTExprFuncCall *expr) override;
	virtual void visit(MxAST::ASTStatementReturn *stat) override;
	virtual void visit(MxAST::ASTStatementBreak *stat) override;
	virtual void visit(MxAST::ASTStatementContinue *stat) override;
	virtual void visit(MxAST::ASTStatementIf *stat) override;
	virtual void visit(MxAST::ASTStatementWhile *stat) override;
	virtual void visit(MxAST::ASTStatementFor *stat) override;
	virtual void visit(MxAST::ASTStatementExpr *stat) override;
	virtual void visit(MxAST::ASTBlock *block) override;

	void generateFuncCall(size_t funcID, const std::vector<MxAST::ASTExpr *> &param);
	void visitExprRec(MxAST::ASTNode *node);	//must be called by visit(ASTExpr* *)
	void visitExpr(MxAST::ASTNode *node);
	void clearXValueStack();

	void releaseLocalVar();

	size_t findMain();

protected:
	MxProgram *program;
	GlobalSymbol *symbol;
	IssueCollector *issues;
	std::list<MxIR::Instruction> lastIns;
	std::shared_ptr<MxIR::Block> lastBlockIn, lastBlockOut;
	std::shared_ptr<MxIR::Block> loopContinue, loopBreak;
	std::shared_ptr<MxIR::Block> returnBlock;
	std::stack<std::pair<MxIR::Operand, MxType>> stkXValues;

	MxIR::Operand lastOperand, lastWriteAddr;	//when Write flag is set and lastWriteAddr.type == empty, we can write to lastOperand directly
	size_t regNum, regThis;

	std::map<size_t, size_t> mapStringConstID;

	size_t funcID;
	std::vector<size_t> declaredVar;

	enum vflag : std::uint32_t
	{ 
		Read = 1, Write = 2 
	};
	std::uint32_t visFlag;
	std::stack<std::uint32_t> stkFlags;
	void setFlag(std::uint32_t newFlag) { stkFlags.push(visFlag); visFlag = newFlag; }
	void resumeFlag() { visFlag = stkFlags.top(); stkFlags.pop(); }
};

#endif