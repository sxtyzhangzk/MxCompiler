#ifndef MX_COMPILER_SSA_H
#define MX_COMPILER_SSA_H

#include "common.h"
#include "IR.h"
#include "MxProgram.h"
#include "utils/DomTree.h"

namespace MxIR
{
	class SSAConstructor
	{
	public:
		SSAConstructor(Function &func) : func(func) {}

	public:
		struct varDefUse
		{
			std::pair<MxIR::InstructionBase *, Operand *> def;
			std::multimap<MxIR::InstructionBase *, Operand *> uses;
			varDefUse() : def({ nullptr, nullptr }) {}
		};
		void constructSSA();
		void constructSSIFull();
		std::map<Operand, varDefUse> calculateDefUse();

		static void constructSSA(MxProgram *program);

	protected:
		void preprocess();
		DomTree getDomTree(bool postDom);
		void renameVar();
		static void renameVar(Block *block, std::set<Block *> &visited, std::vector<size_t> &varCount, std::vector<std::stack<Operand>> &varCurVersion);

	protected:
		struct globalVar	//global: in function
		{
			Operand operand;	//type must be reg
			std::set<size_t> def;		//block id of definition
			std::set<size_t> uses;
		};

		size_t maxVarID;
		std::map<size_t, globalVar> vars;	//varid (regid) -> globalVar
		std::vector<Block *> blocks;		//id -> block
		std::map<Block *, size_t> mapBlock;	//block -> id
		Function &func;
	};
}

#endif