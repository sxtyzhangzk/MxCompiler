#ifndef MX_COMPILER_MEMBER_TABLE_H
#define MX_COMPILER_MEMBER_TABLE_H

#include "common.h"
#include "IR.h"

enum FuncAttribute : std::uint32_t
{
	NoSideEffect = 1,
	ConstExpr = 2,		//No global var read/write
	Linear = 4,			//No cycle
	Builtin = 8,
	ForceInline = 16,
	Export = 32,		//use 'global' to export in NASM; export name is the same as funcName; multiple export function with the same name is not allowed
};

class MxProgram
{
public:
	struct funcInfo
	{
		size_t funcName;
		MxType retType;
		std::vector<MxType> paramType;
		std::uint32_t attribute;
		bool isThiscall;
		std::set<size_t> dependency;		//builtin function not included

		MxIR::Function content;
	};
	struct varInfo
	{
		size_t varName;
		MxType varType;
		std::int64_t offset;	//offset in class
	};
	struct classInfo
	{
		size_t classSize;
		std::vector<varInfo> members;
	};
	struct constInfo
	{
		size_t labelOffset; //must be in range [0, data.size() )
		size_t align;
		std::vector<std::uint8_t> data;
	};

	std::vector<funcInfo> vFuncs;
	std::vector<std::vector<size_t>> vOverloadedFuncs;
	std::vector<varInfo> vGlobalVars;	//Note: A function is also a variable
	std::map<size_t, classInfo> vClass;	//TODO: Compare performance with unordered_map
	std::vector<std::vector<varInfo>> vLocalVars;
	std::vector<constInfo> vConst;

	void setDefault() { defProg = this; }
	static MxProgram * getDefault() { return defProg; }
	
protected:
	static MxProgram *defProg;
};

#endif