#ifndef MX_COMPILER_MX_BUILTIN_H
#define MX_COMPILER_MX_BUILTIN_H

#include "common.h"
#include "GlobalSymbol.h"
#include "MemberTable.h"

enum class MxBuiltin : size_t
{
	print = 0, 
	println = 1,
	getString = 2,
	getInt = 3,
	toString = 4,
	length = 5,
	substring = 6,
	parseInt = 7,
	ord = 8,
	size = 9,

	END
};
enum class MxBuiltinClass : size_t
{
	string = 10,
	array = 11
};

void fillBuiltinSymbol(GlobalSymbol *symbol);
void fillBuiltinMemberTable(MemberTable *memTable);
size_t getBuiltinClassByType(MxType type);

#endif