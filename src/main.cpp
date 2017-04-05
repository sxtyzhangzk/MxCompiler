#include <MxLexer.h>
#include <MxParser.h>
#include <iostream>
#include "AST.h"
#include "ASTConstructor.h"
#include "IssueCollector.h"
#include "StaticTypeChecker.h"
#include "MxBuiltin.h"
using namespace std;

int compile(const std::string &fileName)
{
	antlr4::ANTLRFileStream fin(fileName);
	MxLexer lexer(&fin);
	antlr4::CommonTokenStream tokens(&lexer);
	MxParser parser(&tokens);
	auto prog = parser.prog();

	if (lexer.getNumberOfSyntaxErrors() > 0 || parser.getNumberOfSyntaxErrors() > 0)
		return 1;
	IssueCollector ic(IssueCollector::NOTICE, &cerr, &tokens, fileName);
	ASTConstructor constructor(&ic);
	GlobalSymbol symbol;
	fillBuiltinSymbol(&symbol);

	try
	{
		std::unique_ptr<MxAST::ASTRoot> root(constructor.constructAST(prog, &symbol));
		MemberTable memTable;
		StaticTypeChecker checker(&memTable, &symbol, &ic);
		if (!checker.preCheck(root.get()))
			return 2;
		root->recursiveAccess(&checker);

		//Find main function, temporary implement
		bool foundMain = false;
		for (auto &var : memTable.vGlobalVars)
		{
			if (symbol.vSymbol[var.varName] != "main" || var.varType.mainType != MxType::Function)
				continue;
			for (auto &func : memTable.vOverloadedFuncs[var.varType.funcOLID])
				if (memTable.vFuncs[func].retType.mainType == MxType::Integer)
				{
					foundMain = true;
					break;
				}
			if (foundMain)
				break;
		}
		if (!foundMain)
			ic.error(0, -1, "main function not found");
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
	if (argc <= 1)
	{
		cerr << "Missing argument" << endl;
		return -1;
	}
	int ret = compile(argv[1]);
	if (ret != 0)
		cerr << "compilation terminated" << endl;
	return ret;
}