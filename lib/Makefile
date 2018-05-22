CPP := g++

CFLAGS_O2 := -Isrc/ -Igenerated/ -Iinclude/ -std=c++14 -O2 -DNDEBUG
CFLAGS := -Isrc/ -Igenerated/ -Iinclude/ -std=c++14 -O1 -DNDEBUG
CFLAGS_O0 := -Isrc/ -Igenerated/ -Iinclude/ -std=c++14 -DNDEBUG
LDFLAGS := -static

mxcompiler: libantlr4-runtime.a antlr_generated.a libboost_program_options.a common_headers.h.gch option_parser.o ASM_x64.o AST.o ASTConstructor.o CodeGenerator.o CodeGeneratorBasic.o ConstantFold.o DeadCodeElimination.o GlobalSymbol.o GVN.o InlineOptimizer.o InstructionSelect.o IR.o IRGenerator.o IssueCollector.o LoadCombine.o LoopDetector.o LoopInvariantOptimizer.o main.o MxBuiltin.o MxProgram.o RegisterAllocatorSSA.o SSAConstructor.o SSAReconstructor.o StaticTypeChecker.o CycleEquiv.o DomTree.o MaxClique.o
	$(CPP) $(LDFLAGS) option_parser.o ASM_x64.o AST.o ASTConstructor.o CodeGenerator.o CodeGeneratorBasic.o ConstantFold.o DeadCodeElimination.o GlobalSymbol.o GVN.o InlineOptimizer.o InstructionSelect.o IR.o IRGenerator.o IssueCollector.o LoadCombine.o LoopDetector.o LoopInvariantOptimizer.o main.o MxBuiltin.o MxProgram.o RegisterAllocatorSSA.o SSAConstructor.o SSAReconstructor.o StaticTypeChecker.o CycleEquiv.o DomTree.o MaxClique.o antlr_generated.a libantlr4-runtime.a libboost_program_options.a -o mxcompiler

common_headers.h.gch: ../src/common_headers.h
	$(CPP) ../src/common_headers.h -o common_headers.h.gch $(CFLAGS)

ASM_x64.o: ../src/ASM_x64.cpp
	$(CPP) -c ../src/ASM_x64.cpp -o ASM_x64.o $(CFLAGS_O0)
AST.o: ../src/AST.cpp
	$(CPP) -c ../src/AST.cpp -o AST.o $(CFLAGS)
ASTConstructor.o: ../src/ASTConstructor.cpp
	$(CPP) -c ../src/ASTConstructor.cpp -o ASTConstructor.o $(CFLAGS)
CodeGenerator.o: ../src/CodeGenerator.cpp
	$(CPP) -c ../src/CodeGenerator.cpp -o CodeGenerator.o $(CFLAGS)
CodeGeneratorBasic.o: ../src/CodeGeneratorBasic.cpp
	$(CPP) -c ../src/CodeGeneratorBasic.cpp -o CodeGeneratorBasic.o $(CFLAGS)
ConstantFold.o: ../src/ConstantFold.cpp
	$(CPP) -c ../src/ConstantFold.cpp -o ConstantFold.o $(CFLAGS_O0)
DeadCodeElimination.o: ../src/DeadCodeElimination.cpp
	$(CPP) -c ../src/DeadCodeElimination.cpp -o DeadCodeElimination.o $(CFLAGS_O2)
GlobalSymbol.o: ../src/GlobalSymbol.cpp
	$(CPP) -c ../src/GlobalSymbol.cpp -o GlobalSymbol.o $(CFLAGS_O0)
GVN.o: ../src/GVN.cpp
	$(CPP) -c ../src/GVN.cpp -o GVN.o $(CFLAGS_O2)
InlineOptimizer.o: ../src/InlineOptimizer.cpp
	$(CPP) -c ../src/InlineOptimizer.cpp -o InlineOptimizer.o $(CFLAGS)
InstructionSelect.o: ../src/InstructionSelect.cpp
	$(CPP) -c ../src/InstructionSelect.cpp -o InstructionSelect.o $(CFLAGS)
IR.o: ../src/IR.cpp
	$(CPP) -c ../src/IR.cpp -o IR.o $(CFLAGS)
IRGenerator.o: ../src/IRGenerator.cpp
	$(CPP) -c ../src/IRGenerator.cpp -o IRGenerator.o $(CFLAGS)
IssueCollector.o: ../src/IssueCollector.cpp
	$(CPP) -c ../src/IssueCollector.cpp -o IssueCollector.o $(CFLAGS_O0)
LoadCombine.o: ../src/LoadCombine.cpp
	$(CPP) -c ../src/LoadCombine.cpp -o LoadCombine.o $(CFLAGS)
LoopDetector.o: ../src/LoopDetector.cpp
	$(CPP) -c ../src/LoopDetector.cpp -o LoopDetector.o $(CFLAGS)
LoopInvariantOptimizer.o: ../src/LoopInvariantOptimizer.cpp
	$(CPP) -c ../src/LoopInvariantOptimizer.cpp -o LoopInvariantOptimizer.o $(CFLAGS_O2)
main.o: ../src/main.cpp
	$(CPP) -c ../src/main.cpp -o main.o $(CFLAGS_O0)
MxBuiltin.o: ../src/MxBuiltin.cpp
	$(CPP) -c ../src/MxBuiltin.cpp -o MxBuiltin.o $(CFLAGS)
MxProgram.o: ../src/MxProgram.cpp
	$(CPP) -c ../src/MxProgram.cpp -o MxProgram.o $(CFLAGS_O0)
RegisterAllocatorSSA.o: ../src/RegisterAllocatorSSA.cpp
	$(CPP) -c ../src/RegisterAllocatorSSA.cpp -o RegisterAllocatorSSA.o $(CFLAGS_O2)
SSAConstructor.o: ../src/SSAConstructor.cpp
	$(CPP) -c ../src/SSAConstructor.cpp -o SSAConstructor.o $(CFLAGS)
SSAReconstructor.o: ../src/SSAReconstructor.cpp
	$(CPP) -c ../src/SSAReconstructor.cpp -o SSAReconstructor.o $(CFLAGS)
StaticTypeChecker.o: ../src/StaticTypeChecker.cpp
	$(CPP) -c ../src/StaticTypeChecker.cpp -o StaticTypeChecker.o $(CFLAGS)
CycleEquiv.o: ../src/utils/CycleEquiv.cpp
	$(CPP) -c ../src/utils/CycleEquiv.cpp -o CycleEquiv.o $(CFLAGS_O2)
DomTree.o: ../src/utils/DomTree.cpp
	$(CPP) -c ../src/utils/DomTree.cpp -o DomTree.o $(CFLAGS_O2)
MaxClique.o: ../src/utils/MaxClique.cpp
	$(CPP) -c ../src/utils/MaxClique.cpp -o MaxClique.o $(CFLAGS_O2)
