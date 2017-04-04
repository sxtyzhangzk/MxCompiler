#ifndef MX_COMPILER_GLOBAL_SYMBOL_H
#define MX_COMPILER_GLOBAL_SYMBOL_H

#include "common.h"
#include <unordered_map>
#include <vector>
#include <string>

struct GlobalSymbol
{
	std::unordered_map<std::string, size_t> mapSymbol, mapString;
	std::vector<std::string> vSymbol, vString;

	size_t addSymbol(const std::string &name)
	{
		auto iter = mapSymbol.find(name);
		if (iter == mapSymbol.end())
		{
			vSymbol.push_back(name);
			mapSymbol.insert({ name, vSymbol.size() - 1 });
			return vSymbol.size() - 1;
		}
		return iter->second;
	}
	size_t addString(const std::string &name)
	{
		auto iter = mapString.find(name);
		if (iter == mapString.end())
		{
			vString.push_back(name);
			mapString.insert({ name, vString.size() - 1 });
			return vString.size() - 1;
		}
		return iter->second;
	}
};

#endif
