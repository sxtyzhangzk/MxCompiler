#include "common_headers.h"
#include <MxLexer.h>
#include <MxParser.h>
#include "AST.h"
#include "ASTConstructor.h"
#include "IssueCollector.h"
#include "StaticTypeChecker.h"
#include "MxBuiltin.h"
#include "ConstantFold.h"
#include "IRGenerator.h"
#include "CodeGeneratorBasic.h"
#include "option_parser.h"
#include "SSAConstructor.h"
#include "CodeGenerator.h"
#include "InlineOptimizer.h"
#include "LoopInvariantOptimizer.h"
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

		if (CompileFlags::getInstance()->optim_inline)
		{
			MxIR::InlineOptimizer optim;
			optim.work();
		}

		if (CompileFlags::getInstance()->optim_register_allocation)
		{
			MxIR::SSAConstructor::constructSSA(&program);
			if (CompileFlags::getInstance()->optim_loop_invariant)
			{
				for (auto &func : program.vFuncs)
				{
					MxIR::LoopInvariantOptimizer optim(func.content);
					optim.work();
				}
			}
			std::ofstream fout(output);
			CodeGenerator codegen(fout);
			codegen.generateProgram();
		}
		else
		{
			std::ofstream fout(output);
			CodeGeneratorBasic codegen(fout);
			codegen.generateProgram();
		}
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
	int ret;
	std::string input, output;
	std::tie(ret, input, output) = ParseOptions(argc, argv);
	if (input.empty() || output.empty())
		return ret;
	ret = compile(input, output);
	return ret;
}