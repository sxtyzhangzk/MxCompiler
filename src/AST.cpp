#include "AST.h"

namespace MxAST
{
	void ASTVisitor::visit(ASTBlock *block) {}
	void ASTVisitor::visit(ASTRoot *root) {}
	void ASTVisitor::visit(ASTDeclClass *declClass) {}
	void ASTVisitor::visit(ASTDeclVar *declVar) {}
	void ASTVisitor::visit(ASTDeclFunc *declFunc) { visit(static_cast<ASTBlock *>(declFunc)); }
	void ASTVisitor::visit(ASTExpr *expr) {}
	void ASTVisitor::visit(ASTExprImm *imm) { visit(static_cast<ASTExpr *>(imm)); }
	void ASTVisitor::visit(ASTExprVar *var) { visit(static_cast<ASTExpr *>(var)); }
	void ASTVisitor::visit(ASTExprUnary *unary) { visit(static_cast<ASTExpr *>(unary)); }
	void ASTVisitor::visit(ASTExprBinary *binary) { visit(static_cast<ASTExpr *>(binary)); }
	void ASTVisitor::visit(ASTExprAssignment *assign) { visit(static_cast<ASTExpr *>(assign)); }
	void ASTVisitor::visit(ASTExprNew *exprNew) { visit(static_cast<ASTExpr *>(exprNew)); }
	void ASTVisitor::visit(ASTExprSubscriptAccess *exprSub) { visit(static_cast<ASTExpr *>(exprSub)); }
	void ASTVisitor::visit(ASTExprMemberAccess *expr) { visit(static_cast<ASTExpr *>(expr)); }
	void ASTVisitor::visit(ASTExprFuncCall *expr) { visit(static_cast<ASTExpr *>(expr)); }
	void ASTVisitor::visit(ASTStatement *stat) {}
	void ASTVisitor::visit(ASTStatementReturn *stat) { visit(static_cast<ASTStatement *>(stat)); }
	void ASTVisitor::visit(ASTStatementBreak *stat) { visit(static_cast<ASTStatement *>(stat)); }
	void ASTVisitor::visit(ASTStatementContinue *stat) { visit(static_cast<ASTStatement *>(stat)); }
	void ASTVisitor::visit(ASTStatementIf *stat) { defaultVisit<ASTStatement, ASTBlock>(stat); }
	void ASTVisitor::visit(ASTStatementWhile *stat) { defaultVisit<ASTStatement, ASTBlock>(stat); }
	void ASTVisitor::visit(ASTStatementFor *stat) { defaultVisit<ASTStatement, ASTBlock>(stat); }
	void ASTVisitor::visit(ASTStatementBlock *block) { defaultVisit<ASTStatement, ASTBlock>(block); }
	void ASTVisitor::visit(ASTStatementExpr *stat) { visit(static_cast<ASTStatement *>(stat)); }
	void ASTVisitor::visit(ASTDeclVarLocal *declVar) { defaultVisit<ASTStatement, ASTDeclVar>(declVar); }
	void ASTVisitor::visit(ASTDeclVarGlobal *declVar) { visit(static_cast<ASTDeclVar *>(declVar)); }

	ASTNode * ASTListener::enter(ASTBlock *block) { return block; }
	ASTNode * ASTListener::enter(ASTDeclClass *declClass) { return declClass; }
	ASTNode * ASTListener::enter(ASTDeclVar *declVar) { return declVar; }
	ASTNode * ASTListener::enter(ASTDeclFunc *declFunc) { return enter(static_cast<ASTBlock *>(declFunc)); }
	ASTNode * ASTListener::enter(ASTExpr *expr) { return expr; }
	ASTNode * ASTListener::enter(ASTExprImm *expr) { return enter(static_cast<ASTExpr *>(expr)); }
	ASTNode * ASTListener::enter(ASTExprVar *expr) { return enter(static_cast<ASTExpr *>(expr)); }
	ASTNode * ASTListener::enter(ASTExprUnary *expr) { return enter(static_cast<ASTExpr *>(expr)); }
	ASTNode * ASTListener::enter(ASTExprBinary *expr) { return enter(static_cast<ASTExpr *>(expr)); }
	ASTNode * ASTListener::enter(ASTExprAssignment *expr) { return enter(static_cast<ASTExpr *>(expr)); }
	ASTNode * ASTListener::enter(ASTExprNew *expr) { return enter(static_cast<ASTExpr *>(expr)); }
	ASTNode * ASTListener::enter(ASTExprSubscriptAccess *expr) { return enter(static_cast<ASTExpr *>(expr)); }
	ASTNode * ASTListener::enter(ASTExprMemberAccess *expr) { return enter(static_cast<ASTExpr *>(expr)); }
	ASTNode * ASTListener::enter(ASTExprFuncCall *expr) { return enter(static_cast<ASTExpr *>(expr)); }
	ASTNode * ASTListener::enter(ASTStatement *stat) { return stat; }
	ASTNode * ASTListener::enter(ASTStatementReturn *stat) { return enter(static_cast<ASTStatement *>(stat)); }
	ASTNode * ASTListener::enter(ASTStatementBreak *stat) { return enter(static_cast<ASTStatement *>(stat)); }
	ASTNode * ASTListener::enter(ASTStatementContinue *stat) { return enter(static_cast<ASTStatement *>(stat)); }
	ASTNode * ASTListener::enter(ASTStatementIf *stat) { return defaultEnter<ASTStatement, ASTBlock>(stat); }
	ASTNode * ASTListener::enter(ASTStatementWhile *stat) { return defaultEnter<ASTStatement, ASTBlock>(stat); }
	ASTNode * ASTListener::enter(ASTStatementFor *stat) { return defaultEnter<ASTStatement, ASTBlock>(stat); }
	ASTNode * ASTListener::enter(ASTStatementBlock *stat) { return defaultEnter<ASTStatement, ASTBlock>(stat); }
	ASTNode * ASTListener::enter(ASTStatementExpr *stat) { return enter(static_cast<ASTStatement *>(stat)); }
	ASTNode * ASTListener::enter(ASTDeclVarLocal *declVar) { return defaultEnter<ASTStatement, ASTDeclVar>(declVar); }
	ASTNode * ASTListener::enter(ASTDeclVarGlobal *declVar) { return enter(static_cast<ASTDeclVar *>(declVar)); }

	ASTNode * ASTListener::leave(ASTBlock *block) { return block; }
	ASTNode * ASTListener::leave(ASTDeclClass *declClass) { return declClass; }
	ASTNode * ASTListener::leave(ASTDeclVar *declVar) { return declVar;}
	ASTNode * ASTListener::leave(ASTDeclFunc *declFunc) { return leave(static_cast<ASTBlock *>(declFunc)); }
	ASTNode * ASTListener::leave(ASTExpr *expr) { return expr; }
	ASTNode * ASTListener::leave(ASTExprImm *expr) { return leave(static_cast<ASTExpr *>(expr)); }
	ASTNode * ASTListener::leave(ASTExprVar *expr) { return leave(static_cast<ASTExpr *>(expr)); }
	ASTNode * ASTListener::leave(ASTExprUnary *expr) { return leave(static_cast<ASTExpr *>(expr)); }
	ASTNode * ASTListener::leave(ASTExprBinary *expr) { return leave(static_cast<ASTExpr *>(expr)); }
	ASTNode * ASTListener::leave(ASTExprAssignment *expr) { return leave(static_cast<ASTExpr *>(expr)); }
	ASTNode * ASTListener::leave(ASTExprNew *expr) { return leave(static_cast<ASTExpr *>(expr)); }
	ASTNode * ASTListener::leave(ASTExprSubscriptAccess *expr) { return leave(static_cast<ASTExpr *>(expr)); }
	ASTNode * ASTListener::leave(ASTExprMemberAccess *expr) { return leave(static_cast<ASTExpr *>(expr)); }
	ASTNode * ASTListener::leave(ASTExprFuncCall *expr) { return leave(static_cast<ASTExpr *>(expr)); }
	ASTNode * ASTListener::leave(ASTStatement *stat) { return stat; }
	ASTNode * ASTListener::leave(ASTStatementReturn *stat) { return leave(static_cast<ASTStatement *>(stat)); }
	ASTNode * ASTListener::leave(ASTStatementBreak *stat) { return leave(static_cast<ASTStatement *>(stat)); }
	ASTNode * ASTListener::leave(ASTStatementContinue *stat) { return leave(static_cast<ASTStatement *>(stat)); }
	ASTNode * ASTListener::leave(ASTStatementIf *stat) { return defaultLeave<ASTStatement, ASTBlock>(stat); }
	ASTNode * ASTListener::leave(ASTStatementWhile *stat) { return defaultLeave<ASTStatement, ASTBlock>(stat); }
	ASTNode * ASTListener::leave(ASTStatementFor *stat) { return defaultLeave<ASTStatement, ASTBlock>(stat); }
	ASTNode * ASTListener::leave(ASTStatementBlock *stat) { return defaultLeave<ASTStatement, ASTBlock>(stat); }
	ASTNode * ASTListener::leave(ASTStatementExpr *stat) { return leave(static_cast<ASTStatement *>(stat)); }
	ASTNode * ASTListener::leave(ASTDeclVarLocal *declVar) { return defaultLeave<ASTStatement, ASTDeclVar>(declVar); }
	ASTNode * ASTListener::leave(ASTDeclVarGlobal *declVar) { return leave(static_cast<ASTDeclVar *>(declVar)); }

}