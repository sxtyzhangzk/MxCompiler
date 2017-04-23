CFLAGS := -Isrc/ -Igenerated/ -Iinclude/ -std=c++14 -O1 -DNDEBUG

mxcompiler: libantlr4-runtime.a MxLexer.o MxParser.o MxParserBaseVisitor.o MxParserVisitor.o AST.o ASTConstructor.o CodeGeneratorBasic.o ConstantFold.o GlobalSymbol.o IRGenerator.o IssueCollector.o main.o MxBuiltin.o MxProgram.o StaticTypeChecker.o
	g++ MxLexer.o MxParser.o MxParserBaseVisitor.o MxParserVisitor.o AST.o ASTConstructor.o CodeGeneratorBasic.o ConstantFold.o GlobalSymbol.o IRGenerator.o IssueCollector.o main.o MxBuiltin.o MxProgram.o StaticTypeChecker.o libantlr4-runtime.a libboost_program_options.a -o mxcompiler 

MxLexer.o: generated/MxLexer.cpp
	g++ -c generated/MxLexer.cpp -o MxLexer.o $(CFLAGS)
MxParser.o: generated/MxParser.cpp generated/MxParser.h
	g++ -c generated/MxParser.cpp -o MxParser.o $(CFLAGS)
MxParserBaseVisitor.o: generated/MxParserBaseVisitor.cpp
	g++ -c generated/MxParserBaseVisitor.cpp -o MxParserBaseVisitor.o $(CFLAGS)
MxParserVisitor.o: generated/MxParserVisitor.cpp
	g++ -c generated/MxParserVisitor.cpp -o MxParserVisitor.o $(CFLAGS)

AST.o: ../src/AST.cpp
	g++ -c ../src/AST.cpp -o AST.o $(CFLAGS)
ASTConstructor.o: ../src/ASTConstructor.cpp
	g++ -c ../src/ASTConstructor.cpp -o ASTConstructor.o $(CFLAGS)
CodeGeneratorBasic.o: ../src/CodeGeneratorBasic.cpp
	g++ -c ../src/CodeGeneratorBasic.cpp -o CodeGeneratorBasic.o $(CFLAGS)
ConstantFold.o: ../src/ConstantFold.cpp
	g++ -c ../src/ConstantFold.cpp -o ConstantFold.o $(CFLAGS)
GlobalSymbol.o: ../src/GlobalSymbol.cpp
	g++ -c ../src/GlobalSymbol.cpp -o GlobalSymbol.o $(CFLAGS)
IRGenerator.o: ../src/IRGenerator.cpp
	g++ -c ../src/IRGenerator.cpp -o IRGenerator.o $(CFLAGS)
IssueCollector.o: ../src/IssueCollector.cpp
	g++ -c ../src/IssueCollector.cpp -o IssueCollector.o $(CFLAGS)
main.o: ../src/main.cpp
	g++ -c ../src/main.cpp -o main.o $(CFLAGS)
MxBuiltin.o: ../src/MxBuiltin.cpp
	g++ -c ../src/MxBuiltin.cpp -o MxBuiltin.o $(CFLAGS)
MxProgram.o: ../src/MxProgram.cpp
	g++ -c ../src/MxProgram.cpp -o MxProgram.o $(CFLAGS)
StaticTypeChecker.o: ../src/StaticTypeChecker.cpp
	g++ -c ../src/StaticTypeChecker.cpp -o StaticTypeChecker.o $(CFLAGS)


