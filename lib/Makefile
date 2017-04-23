CFLAGS := -Isrc/ -Igenerated/ -Iinclude/ -std=c++14 -O1 -DNDEBUG
CFLAGS_O0 := -Isrc/ -Igenerated/ -Iinclude/ -std=c++14 -DNDEBUG

mxcompiler: libantlr4-runtime.a antlr_generated.a libboost_program_options.a common_headers.h.gch option_parser.o AST.o ASTConstructor.o CodeGeneratorBasic.o ConstantFold.o GlobalSymbol.o IRGenerator.o IssueCollector.o main.o MxBuiltin.o MxProgram.o StaticTypeChecker.o
	g++ option_parser.o AST.o ASTConstructor.o CodeGeneratorBasic.o ConstantFold.o GlobalSymbol.o IRGenerator.o IssueCollector.o main.o MxBuiltin.o MxProgram.o StaticTypeChecker.o antlr_generated.a libantlr4-runtime.a libboost_program_options.a -o mxcompiler 

common_headers.h.gch: ../src/common_headers.h
	g++ ../src/common_headers.h -o common_headers.h.gch $(CFLAGS)

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
	g++ -c ../src/IssueCollector.cpp -o IssueCollector.o $(CFLAGS_O0)
main.o: ../src/main.cpp
	g++ -c ../src/main.cpp -o main.o $(CFLAGS_O0)
MxBuiltin.o: ../src/MxBuiltin.cpp
	g++ -c ../src/MxBuiltin.cpp -o MxBuiltin.o $(CFLAGS)
MxProgram.o: ../src/MxProgram.cpp
	g++ -c ../src/MxProgram.cpp -o MxProgram.o $(CFLAGS)
StaticTypeChecker.o: ../src/StaticTypeChecker.cpp
	g++ -c ../src/StaticTypeChecker.cpp -o StaticTypeChecker.o $(CFLAGS)


