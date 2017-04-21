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
	std::vector<size_t> vStringRefCount;
	size_t sumStringSize, memoryUsage;

	static GlobalSymbol *defGS;

	GlobalSymbol() : sumStringSize(0), memoryUsage(0) {}

	void setDefault() { defGS = this; }
	static GlobalSymbol * getDefault() { return defGS; }

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
			vStringRefCount.push_back(1);
			sumStringSize += name.size();
			memoryUsage += name.size();
			mapString.insert({ name, vString.size() - 1 });
			return vString.size() - 1;
		}
		if (incStringRef(iter->second) == 1)
			sumStringSize += name.size();
		return iter->second;
	}
	size_t incStringRef(size_t strId)
	{
		return ++vStringRefCount.at(strId);
	}
	size_t decStringRef(size_t strId)
	{
		size_t ref = --vStringRefCount.at(strId);
		if (ref == 0)
			sumStringSize -= vString.at(strId).size();
		return ref;
	}
};

#endif
