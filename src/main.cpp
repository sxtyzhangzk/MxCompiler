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
#include <boost/program_options.hpp>
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
	using namespace boost::program_options;
	options_description options("Options:");
	options.add_options()
		("help,h", "Display this information")
		("input", value<std::vector<std::string>>()->value_name("file"), "Input file")
		("output,o", value<std::string>()->value_name("file"), "Place the output into <file>")
		("fdisable-access-protect", "Set the flag of disable access protect");

	positional_options_description po;
	po.add("input", 1);

	variables_map vm;

	try
	{
		store(command_line_parser(argc, argv).options(options).positional(po).run(), vm);
		notify(vm);
	}
	catch (std::exception &e)
	{
		std::cerr << e.what() << std::endl;
		return 1;
	}

	if (vm.count("help"))
	{
		std::cout << "Usage: " << argv[0] << " [options] file" << std::endl;
		std::cout << options;
		return 0;
	}
	if (vm.count("fdisable-access-protect"))
		CompileFlags::getInstance()->disable_access_protect = true;
	if (!vm.count("input"))
	{
		std::cerr << argv[0] << ": no input file" << std::endl;
		return 1;
	}
	std::string input = vm["input"].as<std::vector<std::string>>()[0];
	std::string output = vm.count("output") ? vm["output"].as<std::string>() : "a.out";
	int ret = compile(input, output);
	return ret;
}