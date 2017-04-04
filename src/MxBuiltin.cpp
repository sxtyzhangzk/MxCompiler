#include "MxBuiltin.h"

void fillBuiltinSymbol(GlobalSymbol *symbol)
{
	symbol->vSymbol = { 
		"print", "println", "getString", 
		"getInt", "toString", "length", 
		"substring", "parseInt", "ord", "size", "$string", "$array" };
	for (size_t i = 0; i < symbol->vSymbol.size(); i++)
		symbol->mapSymbol.insert({ symbol->vSymbol[i], i });
}

void fillBuiltinMemberTable(MemberTable *memTable)
{
	memTable->vFuncs = {
		MemberTable::funcInfo{size_t(MxBuiltin::print), MxType{MxType::Void}, {MxType{MxType::String}}},
		MemberTable::funcInfo{size_t(MxBuiltin::println), MxType{MxType::Void}, {MxType{MxType::String}}},
		MemberTable::funcInfo{size_t(MxBuiltin::getString), MxType{MxType::String}, {}},
		MemberTable::funcInfo{size_t(MxBuiltin::getInt), MxType{MxType::Integer}, {}},
		MemberTable::funcInfo{size_t(MxBuiltin::toString), MxType{MxType::String}, {MxType{MxType::Integer}}},
		MemberTable::funcInfo{size_t(MxBuiltin::length), MxType{MxType::Integer}, {}},
		MemberTable::funcInfo{size_t(MxBuiltin::substring), MxType{MxType::String}, {MxType{MxType::Integer}, MxType{MxType::Integer}}},
		MemberTable::funcInfo{size_t(MxBuiltin::parseInt), MxType{MxType::Integer}, {}},
		MemberTable::funcInfo{size_t(MxBuiltin::ord), MxType{MxType::Integer}, {MxType{MxType::Integer}}},
		MemberTable::funcInfo{size_t(MxBuiltin::size), MxType{MxType::Integer}, {}}
	};
	for (size_t i = size_t(MxBuiltin::print); i < size_t(MxBuiltin::END); i++)
		memTable->vOverloadedFuncs.push_back({ i });
	for (size_t i = size_t(MxBuiltin::print); i <= size_t(MxBuiltin::toString); i++)
		memTable->vGlobalVars.push_back(MemberTable::varInfo{ i, MxType{MxType::Function, 0, size_t(-1), i} });

	memTable->vClassMembers.insert({ size_t(MxBuiltinClass::string), std::vector<MemberTable::varInfo>() });
	for (size_t i = size_t(MxBuiltin::length); i <= size_t(MxBuiltin::ord); i++)
		memTable->vClassMembers[size_t(MxBuiltinClass::string)].push_back(MemberTable::varInfo{ i, MxType{MxType::Function, 0, size_t(-1), i} });

	memTable->vClassMembers.insert({ size_t(MxBuiltinClass::array), std::vector<MemberTable::varInfo>() });
	memTable->vClassMembers[size_t(MxBuiltinClass::array)].
		push_back(MemberTable::varInfo{ size_t(MxBuiltin::size), MxType{ MxType::Function, 0, size_t(-1), size_t(MxBuiltin::size) } });
}

size_t getBuiltinClassByType(MxType type)
{
	if (type.arrayDim > 0)
		return size_t(MxBuiltinClass::array);
	if (type.mainType == MxType::String)
		return size_t(MxBuiltinClass::string);
	return -1;
}