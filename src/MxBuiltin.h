#ifndef MX_COMPILER_MX_BUILTIN_H
#define MX_COMPILER_MX_BUILTIN_H

#include "common.h"
#include "GlobalSymbol.h"
#include "MxProgram.h"
#include "IR.h"

class MxBuiltin
{
public:
	enum class BuiltinFunc : size_t
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
		runtime_error = 10,	//void __runtime_error(const char *)
		strcat = 11,		//string __strcat(string, string)
		strcmp = 12,		//int __strcmp(string, string)
		subscript_bool = 13,//ptr (bool[])::operator[](int)
		subscript_int = 14,	//ptr (int[])::operator[](int)
		subscript_object = 15, //ptr (object[])::operator[](int)
		newobject = 16,		//object __newobject(size_t size, size_t typeid)
		release_string = 17,//void __release_string(string)
		release_array_internal = 18,//void __release_array_internal(array, int dim)	//release reference of array that has internal type (int or bool)
		release_array_string = 19,	//void __release_array_string(array, int dim)	//release reference of string array
		release_array_object = 20,	//void __release_array_object(array, int dim)	//release reference of array that has any object type
		release_object = 21,		//void __release_object(object)	//release reference of any object
		addref_object = 22,			//void __addref_object(object)
		newobject_zero = 23,		//object __newobject_zero(size_t size, size_t typeid) //new object and memset to zero
		initialize = 24,			//void __initialize()
	};

public:
	MxBuiltin() : symbol(GlobalSymbol::getDefault()), program(MxProgram::getDefault()) {}
	MxBuiltin(GlobalSymbol *symbol, MxProgram *program) : symbol(symbol), program(program) {}

	static size_t getBuiltinClassByType(MxType type);
	void init();
	
	static MxProgram::constInfo string2Const(const std::string &str);

	void setDefault() { defBI = this; }
	static MxBuiltin * getDefault() { return defBI; }

protected:
	enum class BuiltinSymbol : size_t
	{
		print = 0,
		println,
		getString,
		getInt,
		toString,
		length,
		substring,
		parseInt,
		ord,
		size,
		Cstring,
		Carray,
		strcmp,
		malloc,
		free,
		realloc,
		scanf,
		puts,
		printf,
		putchar,
		getchar,
		snprintf,
		strlen,
		memcpy,
		sscanf,
		fputs,
		Stderr,		//stderr
		exit,
		memset,

		Hruntime_error,
		Hstrcat,
		Hstrcmp,
		Hsubscript_bool,
		Hsubscript_int,
		Hsubscript_object,
		Hnewobject,
		Hrelease_string,
		Hrelease_array_internal,
		Hrelease_array_string,
		Hrelease_array_object,
		Hrelease_object,
		Haddref_object,
		Hnewobject_zero,

		Hinitialize,
	};
	enum class BuiltinConst : size_t
	{
		Percent_d = 0,		//"%d"
		runtime_error = 1,	//"Runtime Error: "
		subscript_out_of_range = 2,	//"Subscript out of range"
		null_ptr = 3,		//"Null pointer"
		bad_allocation = 4,	//"Bad allocation"
		line_break = 5,		//"\n"
		Percend_s = 6,
	};
	enum class BuiltinClass : size_t
	{
		string = 10,
		array = 11
	};
	
protected:
	void fillBuiltinSymbol();
	void fillBuiltinMemberTable();
	MxIR::Function builtin_print();
	MxIR::Function builtin_println();
	MxIR::Function builtin_getString();
	MxIR::Function builtin_getInt();
	MxIR::Function builtin_toString();
	MxIR::Function builtin_length();
	MxIR::Function builtin_substring();
	MxIR::Function builtin_parseInt();
	MxIR::Function builtin_ord_safe();
	MxIR::Function builtin_size();
	MxIR::Function builtin_runtime_error();
	MxIR::Function builtin_strcat();
	MxIR::Function builtin_strcmp();
	MxIR::Function builtin_subscript_safe(size_t size);
	MxIR::Function builtin_newobject();
	MxIR::Function builtin_newobject_zero();

	MxIR::Function builtin_stub();

protected:
	GlobalSymbol *symbol;
	MxProgram *program;
	static MxBuiltin *defBI;
	static const size_t objectHeader = 2 * POINTER_SIZE;
	static const size_t stringHeader = POINTER_SIZE, arrayHeader = POINTER_SIZE;
};

#endif