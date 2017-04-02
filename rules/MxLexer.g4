lexer grammar MxLexer;

BoolType: 'bool';
IntType: 'int';
StringType: 'string';
Null: 'null';
Void: 'void';
True: 'true';
False: 'false';
If: 'if';
Else: 'else';
For: 'for';
While: 'while';
Break: 'break';
Continue: 'continue';
Return: 'return';
New: 'new';
Class: 'class';
This: 'this';

Plus: '+';
Minus: '-';
Multi: '*';
Div: '/';
Mod: '%';
GreaterThan: '>';
LessThan: '<';
Equal: '==';
NotEqual: '!=';
GreaterEqual: '>=';
LessEqual: '<=';

And: '&&';
Or: '||';
Not: '!';
ShiftLeft: '<<';
ShiftRight: '>>';
BitNot: '~';
BitOr: '|';
BitAnd: '&';
BitXor: '^';

Assign: '=';

Increment: '++';
Decrement: '--';

Dot: '.';

OpenPar: '(';
ClosePar: ')';
OpenSqu: '[';
CloseSqu: ']';
OpenCurly: '{';
CloseCurly: '}';

Semicolon: ';';

Comma: ',';

Id: [A-Za-z_][A-Za-z0-9_]*;

IntegerDec: [1-9][0-9]*|'0';

String: '"' (~'\\'|'\\'.)*? '"';

Comment: '//' ~[\r\n]* '\r'? '\n' -> skip;
CommentBlock: '/*' .*? '*/' ->skip;
Whitespace: [ \t\r\n] -> skip;