#include <MxLexer.h>
#include <MxParser.h>
#include <iostream>
#include "AST.h"
#include "ASTConstructor.h"
#include "IssueCollector.h"
#include "StaticTypeChecker.h"
#include "MxBuiltin.h"
#include "ConstantFold.h"
#include "IRGenerator.h"
#include "CodeGeneratorBasic.h"
using namespace std;

int compile(const std::string &fileName, const std::string &output)
{
	antlr4::ANTLRFileStream fin(fileName);
	MxLexer lexer(&fin);
	antlr4::CommonTokenStream tokens(&lexer);
	MxParser parser(&tokens);
	auto prog = parser.prog();

	if (lexer.getNumberOfSyntaxErrors() > 0 || parser.getNumberOfSyntaxErrors() > 0)
		return 1;
	IssueCollector ic(IssueCollector::NOTICE, &cerr, &tokens, fileName);
	ic.setDefault();

	ASTConstructor constructor(&ic);
	GlobalSymbol symbol;
	symbol.setDefault();

	try
	{
		MxProgram program;
		program.setDefault();

		MxBuiltin builtin;
		builtin.setDefault();
		builtin.init();

		std::unique_ptr<MxAST::ASTRoot> root(constructor.constructAST(prog, &symbol));
		
		StaticTypeChecker checker(&program, &symbol, &ic);
		if (!checker.preCheck(root.get()))
			return 2;
		root->recursiveAccess(&checker);

		if (ic.cntError > 0)
			return 2;

		ASTOptimizer::ConstantFold cfold;
		root->recursiveAccess(&cfold);

		IRGenerator irgen;
		irgen.generateProgram(root.get());

		if (ic.cntError > 0)
			return 2;

		std::ofstream fout(output);
		CodeGeneratorBasic codegen(fout);
		codegen.generateProgram();
	}
	catch (IssueCollector::FatalErrorException &)
	{
		return 2;
	}
	if (ic.cntError > 0)
		return 2;
	return 0;
}

int main(int argc, char *argv[])
{
	if (argc <= 2)
	{
		cerr << "Missing argument" << endl;
		return -1;
	}
	int ret = compile(argv[1], argv[2]);
	if (ret != 0)
		cerr << "compilation terminated" << endl;
	return ret;
}