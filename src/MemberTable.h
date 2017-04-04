#ifndef MX_COMPILER_MEMBER_TABLE_H
#define MX_COMPILER_MEMBER_TABLE_H

#include "common.h"
#include <vector>
#include <map>

struct MemberTable
{
	struct funcInfo
	{
		size_t funcName;
		MxType retType;
		std::vector<MxType> paramType;
	};
	struct varInfo
	{
		size_t varName;
		MxType varType;
	};

	std::vector<funcInfo> vFuncs;
	std::vector<std::vector<size_t>> vOverloadedFuncs;
	std::vector<varInfo> vGlobalVars;	//Note: A function is also a variable
	std::map<size_t, std::vector<varInfo>> vClassMembers;	//TODO: Compare performance with unordered_map
	std::vector<std::vector<varInfo>> vLocalVars;
	
};

#endif