#include "common_headers.h"
#include "ASM.h"

std::string regName(int id, size_t size)
{
	static const std::string regTable[8][4] = {
		{ "al", "ax", "eax", "rax" },
		{ "cl", "cx", "ecx", "rcx" },
		{ "dl", "dx", "edx", "rdx" },
		{ "bl", "bx", "ebx", "rbx" },
		{ "spl", "sp", "esp", "rsp" },
		{ "bpl", "bp", "ebp", "rbp" },
		{ "sil", "si", "esi", "rsi" },
		{ "dil", "di", "edi", "rdi" },
	};
	if (id < 8)
	{
		if (size == 1)
			return regTable[id][0];
		if (size == 2)
			return regTable[id][1];
		if (size == 4)
			return regTable[id][2];
		assert(size == 8);
		return regTable[id][3];
	}
	std::stringstream ss;
	ss << "r" << id;
	if (size == 1)
		ss << "b";
	else if (size == 2)
		ss << "w";
	else if (size == 4)
		ss << "d";
	else
	{
		assert(size == 8);
	}
	return ss.str();
}


std::string sizeName(size_t size)
{
	if (size == 1)
		return "byte";
	if (size == 2)
		return "word";
	if (size == 4)
		return "dword";
	assert(size == 8);
	return "qword";
}