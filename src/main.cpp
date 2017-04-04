#include <MxLexer.h>
#include <MxParser.h>
#include <iostream>
#include "AST.h"
#include "ASTConstructor.h"
#include "IssueCollector.h"
#include "StaticTypeChecker.h"
#include "MxBuiltin.h"
using namespace std;


int main(int argc, char *argv[])
{
	if (argc <= 1)
	{
		cerr << "Missing argument" << endl;
		return -1;
	}
	antlr4::ANTLRFileStream fin(argv[1]);
	MxLexer lexer(&fin);
	antlr4::CommonTokenStream tokens(&lexer);
	MxParser parser(&tokens);
	auto prog = parser.prog();

	if (lexer.getNumberOfSyntaxErrors() > 0 || parser.getNumberOfSyntaxErrors() > 0)
	{
		cerr << "compilation terminated" << endl;
		return 1;
	}
	IssueCollector ic(IssueCollector::NOTICE, &cerr, &tokens, argv[1]);
	ASTConstructor constructor(&ic);
	GlobalSymbol symbol;
	fillBuiltinSymbol(&symbol);

	try
	{
		std::unique_ptr<MxAST::ASTRoot> root(constructor.constructAST(prog, &symbol));
		MemberTable memTable;
		StaticTypeChecker checker(&memTable, &symbol, &ic);
		checker.preCheck(root.get());
		root->recursiveAccess(&checker);
		
		//Find main function, temporary implement
		bool foundMain = false;
		for (auto &var : memTable.vGlobalVars)
		{
			if (symbol.vSymbol[var.varName] != "main" || var.varType.mainType != MxType::Function)
				continue;
			for(auto &func : memTable.vOverloadedFuncs[var.varType.funcOLID])
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
		cerr << "compilation terminated" << endl;
		return 2;
	}
	if (ic.cntError > 0)
	{
		cerr << "compilation terminated" << endl;
		return 2;
	}
	return 0;
}