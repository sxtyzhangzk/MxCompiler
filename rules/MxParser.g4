parser grammar MxParser;

options
{
    tokenVocab = MxLexer;
}

exprList: (expr (Comma expr)*)?;
exprNewDim: OpenSqu expr? CloseSqu;

exprPrimary: Id
    | String
    | IntegerDec
    | True
    | False
    | This
    | Null
    | exprPar
    ;

exprPar: OpenPar expr ClosePar;
subexprPostfix: exprPrimary                    #subexpr0
    | subexprPostfix Increment                 #exprIncrementPostfix
    | subexprPostfix Decrement                 #exprDecrementPostfix
    | subexprPostfix Dot Id                    #exprMember
    | subexprPostfix OpenPar exprList ClosePar #exprFuncCall
    | subexprPostfix OpenSqu expr CloseSqu     #exprSubscript
    ;

subexprPrefix: subexprPostfix      #subexpr1
    | Increment subexprPrefix      #exprIncrementPrefix
    | Decrement subexprPrefix      #exprDecrementPrefix
    | Plus subexprPrefix           #exprPositive
    | Minus subexprPrefix          #exprNegative
    | Not subexprPrefix            #exprNot
    | BitNot subexprPrefix         #exprBitNot  
    | New typeNotArray ((OpenPar exprList ClosePar) | (exprNewDim)*)  #exprNew
    ;
subexprMultiDiv: subexprPrefix              #subexpr2
    | subexprMultiDiv Multi subexprPrefix   #exprMulti
    | subexprMultiDiv Div subexprPrefix     #exprDiv
    | subexprMultiDiv Mod subexprPrefix     #exprMod
    ;
subexprPlusMinus: subexprMultiDiv               #subexpr3
    | subexprPlusMinus Plus subexprMultiDiv     #exprPlus
    | subexprPlusMinus Minus subexprMultiDiv    #exprMinus
    ;

subexprShift: subexprPlusMinus                    #subexpr4
    | subexprShift ShiftLeft subexprPlusMinus     #exprShiftLeft
    | subexprShift ShiftRight subexprPlusMinus    #exprShiftRight
    ;
subexprCompareRel: subexprShift                      #subexpr5
    | subexprCompareRel LessThan subexprShift        #exprLessThan
    | subexprCompareRel LessEqual subexprShift       #exprLessEqual
    | subexprCompareRel GreaterThan subexprShift     #exprGreaterThan
    | subexprCompareRel GreaterEqual subexprShift    #exprGreaterEqual
    ;
subexprCompareEqu: subexprCompareRel                  #subexpr6
    | subexprCompareEqu Equal subexprCompareRel       #exprEqual
    | subexprCompareEqu NotEqual subexprCompareRel    #exprNotEqual
    ;

subexprBitand: subexprCompareEqu               #subexpr7
    | subexprBitand BitAnd subexprCompareEqu   #exprBitand
    ;
subexprXor: subexprBitand               #subexpr8
    | subexprXor BitXor subexprBitand   #exprXor
    ;
subexprBitor: subexprXor                #subexpr9
    | subexprBitor BitOr subexprXor     #exprBitor
    ;
subexprAnd: subexprBitor                #subexpr10
    | subexprAnd And subexprBitor       #exprAnd
    ;
subexprOr: subexprAnd                   #subexpr11
    | subexprOr Or subexprAnd           #exprOr
    ;

subexprAssignment: subexprOr                #subexpr12
    | subexprOr Assign subexprAssignment    #exprAssignment
    ;

expr: subexprAssignment;

typeInternal: IntType
    | StringType
    | BoolType
    ;

typeNotArray: (typeInternal | Id);
type: typeNotArray (OpenSqu CloseSqu)*;

varDecl: type Id (Assign expr)? (Comma Id (Assign expr)?)* Semicolon;
paramList: (type Id (Comma type Id)*)?;
funcDecl: (type | Void)? Id OpenPar paramList ClosePar block;
memberList: (varDecl | funcDecl)*;
classDecl: Class Id OpenCurly memberList CloseCurly;
if_statement: If OpenPar expr ClosePar statement (Else statement)?;

for_exprIn: (varDecl | expr? Semicolon);
for_exprCond: expr? Semicolon;
for_exprStep: expr?;
for_statement: For OpenPar for_exprIn for_exprCond for_exprStep ClosePar statement;

while_statement: While OpenPar expr ClosePar statement;
statement: block | if_statement | for_statement | while_statement | varDecl | ((expr | Continue | Break | Return expr?)? Semicolon);
block: OpenCurly statement* CloseCurly;
prog: (classDecl | funcDecl | varDecl)*;