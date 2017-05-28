CFLAGS_O2 := -Isrc/ -Igenerated/ -Iinclude/ -std=c++14 -O2 -DNDEBUG
CFLAGS := -Isrc/ -Igenerated/ -Iinclude/ -std=c++14 -O1 -DNDEBUG
CFLAGS_O0 := -Isrc/ -Igenerated/ -Iinclude/ -std=c++14 -DNDEBUG

mxcompiler: libantlr4-runtime.a antlr_generated.a libboost_program_options.a common_headers.h.gch option_parser.o ASM_x64.o AST.o ASTConstructor.o CodeGenerator.o CodeGeneratorBasic.o ConstantFold.o DeadCodeElimination.o GlobalSymbol.o InlineOptimizer.o IR.o IRGenerator.o IssueCollector.o LoopDetector.o LoopInvariantOptimizer.o main.o MxBuiltin.o MxProgram.o RegisterAllocatorSSA.o SSAConstructor.o SSAReconstructor.o StaticTypeChecker.o CycleEquiv.o DomTree.o MaxClique.o
	g++ option_parser.o ASM_x64.o AST.o ASTConstructor.o CodeGenerator.o CodeGeneratorBasic.o ConstantFold.o DeadCodeElimination.o GlobalSymbol.o InlineOptimizer.o IR.o IRGenerator.o IssueCollector.o LoopDetector.o LoopInvariantOptimizer.o main.o MxBuiltin.o MxProgram.o RegisterAllocatorSSA.o SSAConstructor.o SSAReconstructor.o StaticTypeChecker.o CycleEquiv.o DomTree.o MaxClique.o antlr_generated.a libantlr4-runtime.a libboost_program_options.a -o mxcompiler 

common_headers.h.gch: ../src/common_headers.h
	g++ ../src/common_headers.h -o common_headers.h.gch $(CFLAGS)

ASM_x64.o: ../src/ASM_x64.cpp
	g++ -c ../src/ASM_x64.cpp -o ASM_x64.o $(CFLAGS_O0)
AST.o: ../src/AST.cpp
	g++ -c ../src/AST.cpp -o AST.o $(CFLAGS_O0)
ASTConstructor.o: ../src/ASTConstructor.cpp
	g++ -c ../src/ASTConstructor.cpp -o ASTConstructor.o $(CFLAGS)
CodeGenerator.o: ../src/CodeGenerator.cpp
	g++ -c ../src/CodeGenerator.cpp -o CodeGenerator.o $(CFLAGS)
CodeGeneratorBasic.o: ../src/CodeGeneratorBasic.cpp
	g++ -c ../src/CodeGeneratorBasic.cpp -o CodeGeneratorBasic.o $(CFLAGS)
ConstantFold.o: ../src/ConstantFold.cpp
	g++ -c ../src/ConstantFold.cpp -o ConstantFold.o $(CFLAGS_O0)
DeadCodeElimination.o: ../src/DeadCodeElimination.cpp
	g++ -c ../src/DeadCodeElimination.cpp -o DeadCodeElimination.o $(CFLAGS)
GlobalSymbol.o: ../src/GlobalSymbol.cpp
	g++ -c ../src/GlobalSymbol.cpp -o GlobalSymbol.o $(CFLAGS_O0)
InlineOptimizer.o: ../src/InlineOptimizer.cpp
	g++ -c ../src/InlineOptimizer.cpp -o InlineOptimizer.o $(CFLAGS)
IR.o: ../src/IR.cpp
	g++ -c ../src/IR.cpp -o IR.o $(CFLAGS)
IRGenerator.o: ../src/IRGenerator.cpp
	g++ -c ../src/IRGenerator.cpp -o IRGenerator.o $(CFLAGS)
IssueCollector.o: ../src/IssueCollector.cpp
	g++ -c ../src/IssueCollector.cpp -o IssueCollector.o $(CFLAGS_O0)
LoopDetector.o: ../src/LoopDetector.cpp
	g++ -c ../src/LoopDetector.cpp -o LoopDetector.o $(CFLAGS)
LoopInvariantOptimizer.o: ../src/LoopInvariantOptimizer.cpp
	g++ -c ../src/LoopInvariantOptimizer.cpp -o LoopInvariantOptimizer.o $(CFLAGS)
main.o: ../src/main.cpp
	g++ -c ../src/main.cpp -o main.o $(CFLAGS_O0)
MxBuiltin.o: ../src/MxBuiltin.cpp
	g++ -c ../src/MxBuiltin.cpp -o MxBuiltin.o $(CFLAGS_O0)
MxProgram.o: ../src/MxProgram.cpp
	g++ -c ../src/MxProgram.cpp -o MxProgram.o $(CFLAGS_O0)
RegisterAllocatorSSA.o: ../src/RegisterAllocatorSSA.cpp
	g++ -c ../src/RegisterAllocatorSSA.cpp -o RegisterAllocatorSSA.o $(CFLAGS_O2)
SSAConstructor.o: ../src/SSAConstructor.cpp
	g++ -c ../src/SSAConstructor.cpp -o SSAConstructor.o $(CFLAGS)
SSAReconstructor.o: ../src/SSAReconstructor.cpp
	g++ -c ../src/SSAReconstructor.cpp -o SSAReconstructor.o $(CFLAGS)
StaticTypeChecker.o: ../src/StaticTypeChecker.cpp
	g++ -c ../src/StaticTypeChecker.cpp -o StaticTypeChecker.o $(CFLAGS_O0)
CycleEquiv.o: ../src/utils/CycleEquiv.cpp
	g++ -c ../src/utils/CycleEquiv.cpp -o CycleEquiv.o $(CFLAGS)
DomTree.o: ../src/utils/DomTree.cpp
	g++ -c ../src/utils/DomTree.cpp -o DomTree.o $(CFLAGS)
MaxClique.o: ../src/utils/MaxClique.cpp
	g++ -c ../src/utils/MaxClique.cpp -o MaxClique.o $(CFLAGS_O2)
