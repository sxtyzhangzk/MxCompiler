#include "common_headers.h"
#include "MxBuiltin.h"
using namespace MxIR;

MxBuiltin * MxBuiltin::defBI = nullptr;

void MxBuiltin::init()
{
	fillBuiltinSymbol();
	fillBuiltinMemberTable();
}

MxProgram::constInfo MxBuiltin::string2Const(const std::string &str)
{
	MxProgram::constInfo cinfo;
	cinfo.align = 8;
	cinfo.labelOffset = 16;

	for(int i = 0; i < 8; i++)
		cinfo.data.push_back(0xff);
	for (int i = 0; i < 8; i++)
		cinfo.data.push_back(0);

	std::uint64_t length = str.length();
	for (int i = 0; i < 8; i++)
		cinfo.data.push_back((length >> (i * 8)) & 0xff);

	std::copy(str.begin(), str.end(), std::back_inserter(cinfo.data));
	cinfo.data.push_back(0);

	return cinfo;
}

void MxBuiltin::fillBuiltinSymbol()
{
	symbol->vSymbol = { 
		"print", "println", "getString", 
		"getInt", "toString", "length", 
		"substring", "parseInt", "ord", "size", "$string", "$array", 
		"strcmp", "malloc", "free", "realloc", "scanf", "puts", "printf", "putchar", "getchar", "snprintf", "strlen", "memcpy", "sscanf", "fputs", "stderr", "exit", "memset",
		"__runtime_error", "__strcat", "__strcmp", "__subscript_bool", "__subscript_int", "__subscript_object", "__newobject",
		"__release_string", "__release_array_internal", "__release_array_string", "__release_array_object",
		"__release_object", "__addref_object", "__newobject_zero",
		"__initialize" };
	for (size_t i = 0; i < symbol->vSymbol.size(); i++)
		symbol->mapSymbol.insert({ symbol->vSymbol[i], i });
}

void MxBuiltin::fillBuiltinMemberTable()
{
	bool safe = !CompileFlags::getInstance()->disable_access_protect;
	program->vFuncs = {
		MxProgram::funcInfo{size_t(BuiltinSymbol::print),		MxType{MxType::Void},		{MxType{MxType::String}}, Builtin, false, {}, builtin_print()},
		MxProgram::funcInfo{size_t(BuiltinSymbol::println),		MxType{MxType::Void},		{MxType{MxType::String}}, Builtin, false, {}, builtin_println()},
		MxProgram::funcInfo{size_t(BuiltinSymbol::getString),	MxType{MxType::String},		{}, Builtin, false, {}, builtin_getString()},
		MxProgram::funcInfo{size_t(BuiltinSymbol::getInt),		MxType{MxType::Integer},	{}, Builtin, false, {}, builtin_getInt()},
		MxProgram::funcInfo{size_t(BuiltinSymbol::toString),	MxType{MxType::String},		{MxType{MxType::Integer}}, NoSideEffect | Builtin, false, {}, builtin_toString()},
		MxProgram::funcInfo{size_t(BuiltinSymbol::length),		MxType{MxType::Integer},	{MxType{MxType::String}}, NoSideEffect | Builtin, true, {}, builtin_length()},
		MxProgram::funcInfo{size_t(BuiltinSymbol::substring),	MxType{MxType::String},		{MxType{MxType::String}, MxType{MxType::Integer}, MxType{MxType::Integer}}, NoSideEffect | Builtin, true, {}, builtin_substring()},
		MxProgram::funcInfo{size_t(BuiltinSymbol::parseInt),	MxType{MxType::Integer},	{MxType{MxType::String}}, NoSideEffect | Builtin, true, {}, builtin_parseInt() },
		MxProgram::funcInfo{size_t(BuiltinSymbol::ord),			MxType{MxType::Integer},	{MxType{MxType::String}, MxType{MxType::Integer}}, NoSideEffect | Builtin, true, {}, safe ? builtin_ord_safe() : builtin_ord_unsafe() },
		MxProgram::funcInfo{size_t(BuiltinSymbol::size),		MxType{MxType::Integer},	{MxType{MxType::Object, 0, size_t(-1)}}, NoSideEffect | Builtin, true, {}, safe ? builtin_size() : builtin_size_unsafe() },
		MxProgram::funcInfo{size_t(BuiltinSymbol::Hruntime_error), MxType{MxType::Void},	{MxType{MxType::Object, 0, size_t(-1)}}, Builtin | NoInline, false, {}, builtin_runtime_error()},
		MxProgram::funcInfo{size_t(BuiltinSymbol::Hstrcat),		MxType{MxType::String},		{MxType{MxType::String}, MxType{MxType::String}}, NoSideEffect | Builtin, false, {}, builtin_strcat()},
		MxProgram::funcInfo{size_t(BuiltinSymbol::Hstrcmp),		MxType{MxType::Integer},	{MxType{MxType::String}, MxType{MxType::String}}, NoSideEffect | Builtin, false, {}, builtin_strcmp()},
		MxProgram::funcInfo{size_t(BuiltinSymbol::Hsubscript_bool),	MxType::Null(),				{MxType{MxType::Bool, 1}, MxType{MxType::Integer}}, NoSideEffect | ConstExpr | Builtin | ForceInline, true, {}, safe ? builtin_subscript_safe(1) : builtin_subscript_unsafe(1) },
		MxProgram::funcInfo{size_t(BuiltinSymbol::Hsubscript_int),	MxType::Null(),				{MxType{MxType::Integer, 1 }, MxType{ MxType::Integer } }, NoSideEffect | ConstExpr | Builtin | ForceInline, true,{}, safe ? builtin_subscript_safe(4) : builtin_subscript_unsafe(4) },
		MxProgram::funcInfo{size_t(BuiltinSymbol::Hsubscript_object),	MxType::Null(),				{ MxType{ MxType::Object, 1, size_t(-1)}, MxType{ MxType::Integer } }, NoSideEffect | ConstExpr | Builtin | ForceInline, true,{}, safe ? builtin_subscript_safe(8) : builtin_subscript_unsafe(8)},
		MxProgram::funcInfo{size_t(BuiltinSymbol::Hnewobject),	MxType::Null(),				{MxType::Null(), MxType::Null()}, Builtin, false, {}, builtin_newobject()},
		MxProgram::funcInfo{size_t(BuiltinSymbol::Hrelease_string), MxType{MxType::Void},	{MxType{MxType::String}}, Builtin, false, {}, /*builtin_stub({ RegPtr(0) })*/builtin_release_string()},
		MxProgram::funcInfo{size_t(BuiltinSymbol::Hrelease_array_internal), MxType{MxType::Void}, {MxType::Null()}, Builtin, false, {}, /*builtin_stub({RegPtr(0), Reg32(1)})*/ builtin_release_array(true)/*TODO*/},
		MxProgram::funcInfo{ size_t(BuiltinSymbol::Hrelease_array_string), MxType{ MxType::Void },{ MxType::Null() }, Builtin, false,{}, builtin_stub({ RegPtr(0) })/*TODO*/ },
		MxProgram::funcInfo{ size_t(BuiltinSymbol::Hrelease_array_object), MxType{ MxType::Void },{ MxType::Null() }, Builtin, false,{}, /*builtin_stub({ RegPtr(0), Reg32(1) })*/ builtin_release_array(false)/*TODO*/ },
		MxProgram::funcInfo{ size_t(BuiltinSymbol::Hrelease_object), MxType{ MxType::Void },{ MxType::Null() }, Builtin, false,{}, builtin_stub({ RegPtr(0) })/*TODO*/ },
		MxProgram::funcInfo{ size_t(BuiltinSymbol::Haddref_object), MxType{ MxType::Void },{ MxType::Null() }, Builtin, false,{}, builtin_addref_object() },
		MxProgram::funcInfo{ size_t(BuiltinSymbol::Hnewobject_zero), MxType::Null(), { MxType::Null(), MxType::Null() }, Builtin, false,{}, builtin_newobject_zero() },
		MxProgram::funcInfo{size_t(BuiltinSymbol::Hinitialize), MxType{MxType::Void}, {}, Builtin, false, {}},
	};
	for (size_t i = size_t(BuiltinFunc::print); i <= size_t(BuiltinFunc::size); i++)
		program->vOverloadedFuncs.push_back({ i });
	for (size_t i = size_t(BuiltinFunc::print); i <= size_t(BuiltinFunc::toString); i++)
		program->vGlobalVars.push_back(MxProgram::varInfo{ i, MxType{MxType::Function, 0, size_t(-1), i} });

	program->vClass.insert({ 0, { size_t(BuiltinClass::string), std::vector<MxProgram::varInfo>() } });
	for (size_t i = size_t(BuiltinFunc::length); i <= size_t(BuiltinFunc::ord); i++)
		program->vClass[size_t(BuiltinClass::string)].members.push_back(MxProgram::varInfo{ i, MxType{MxType::Function, 0, size_t(-1), i} });

	program->vClass.insert({ 0, { size_t(BuiltinClass::array), std::vector<MxProgram::varInfo>() } });
	program->vClass[size_t(BuiltinClass::array)].members.
		push_back(MxProgram::varInfo{ size_t(BuiltinFunc::size), MxType{ MxType::Function, 0, size_t(-1), size_t(BuiltinFunc::size) } });

	static const std::string builtinString[] = {
		"%d", "Runtime Error: ", "Subscript out of range", "Null pointer", "Bad allocation", "\n", "%s" };
	for (auto &str : builtinString)
	{
		MxProgram::constInfo cinfo;
		cinfo.align = 1;
		cinfo.labelOffset = 0;
		std::copy(str.begin(), str.end(), std::back_inserter(cinfo.data));
		cinfo.data.push_back(0);
		program->vConst.push_back(std::move(cinfo));
	}
}

size_t MxBuiltin::getBuiltinClassByType(MxType type)
{
	if (type.arrayDim > 0)
		return size_t(BuiltinClass::array);
	if (type.mainType == MxType::String)
		return size_t(BuiltinClass::string);
	return -1;
}

Function MxBuiltin::builtin_print()
{
	std::shared_ptr<Block> block[4];
	for (auto &blk : block)
		blk = Block::construct();

	block[0]->ins = {
		IR(Reg8(1), Seq, RegPtr(0), ImmPtr(0)),
		IRBranch(Reg8(1), unlikely),
	};
	block[0]->brTrue = block[1];
	block[0]->brFalse = block[2];

	block[1]->ins = {	//empty string
		IRReturn(),
	};
	block[1]->brTrue = block[3];

	block[2]->ins = {
		IR(RegPtr(2), Add, RegPtr(0), ImmPtr(stringHeader)),
		IRCall(EmptyOperand(), IDExtSymbol(size_t(BuiltinSymbol::printf)),{ IDConst(size_t(BuiltinConst::Percend_s)), RegPtr(2) }),
		IRReturn(),
	};
	block[2]->brTrue = block[3];

	Function ret;
	ret.params = { RegPtr(0) };
	ret.inBlock = block[0];
	ret.outBlock = block[3];
	return ret;
}

Function MxBuiltin::builtin_println()
{
	std::shared_ptr<Block> block[4];
	for (auto &blk : block)
		blk = Block::construct();

	block[0]->ins = {
		IR(Reg8(1), Seq, RegPtr(0), ImmPtr(0)),
		IRBranch(Reg8(1), unlikely),
	};
	block[0]->brTrue = block[1];
	block[0]->brFalse = block[2];

	block[1]->ins = {	//empty string
		IRCall(EmptyOperand(), IDExtSymbol(size_t(BuiltinSymbol::putchar)), {Imm32('\n')}),
		IRReturn(),
	};
	block[1]->brTrue = block[3];

	block[2]->ins = {
		IR(RegPtr(2), Add, RegPtr(0), ImmPtr(stringHeader)),
		IRCall(EmptyOperand(), IDExtSymbol(size_t(BuiltinSymbol::puts)),{ RegPtr(2) }),
		IRReturn(),
	};
	block[2]->brTrue = block[3];

	Function ret;
	ret.params = { RegPtr(0) };
	ret.inBlock = block[0];
	ret.outBlock = block[3];
	return ret;
}

//FIXME: release reference before returning
Function MxBuiltin::builtin_getString()
{
	static const size_t blockSize = 1024;
	std::shared_ptr<Block> block[11];
	for (auto &blk : block)
		blk = Block::construct();

	block[0]->ins = {
		IRCall(RegPtr(0), IDExtSymbol(size_t(BuiltinSymbol::malloc)), {ImmPtr(objectHeader + stringHeader + blockSize)}),
		IR(RegPtr(1), Move, ImmPtr(objectHeader + stringHeader)),	//current offset
		IR(RegPtr(2), Move, ImmPtr(objectHeader + stringHeader + blockSize)),	//size
		IRJump(),
	};
	block[0]->brTrue = block[1];
	
	block[1]->ins = {
		IRCall(Reg32(3), IDExtSymbol(size_t(BuiltinSymbol::getchar)), {}),
		IR(Reg8(4), Sne, Reg32(3), Imm32(-1)),
		IRBranch(Reg8(4), likely),
	};
	block[1]->brTrue = block[2];
	block[1]->brFalse = block[6];

	block[2]->ins = {
		IR(Reg8(5), Sne, Reg32(3), Imm32(' ')),
		IRBranch(Reg8(5), likely),
	};
	block[2]->brTrue = block[3];
	block[2]->brFalse = block[9];

	block[3]->ins = {
		IR(Reg8(6), Sne, Reg32(3), Imm32('\n')),
		IRBranch(Reg8(6), likely),
	};
	block[3]->brTrue = block[4];
	block[3]->brFalse = block[9];

	block[4]->ins = {
		IRStoreA(Reg8(3), RegPtr(0), RegPtr(1)),
		IR(RegPtr(1), Add, RegPtr(1), ImmPtr(1)),
		IR(Reg8(7), Seq, RegPtr(1), RegPtr(2)),
		IRBranch(Reg8(7), unlikely),
	};
	block[4]->brTrue = block[5];
	block[4]->brFalse = block[1];

	block[5]->ins = {
		IR(RegPtr(2), Shl, RegPtr(2), ImmPtr(1)),
		IRCall(RegPtr(0), IDExtSymbol(size_t(BuiltinSymbol::realloc)), {RegPtr(0), RegPtr(2)}),
		IRJump(),
	};
	block[5]->brTrue = block[1];

	block[6]->ins = {
		IR(Reg8(9), Seq, RegPtr(1), ImmPtr(objectHeader + stringHeader)),		//if read 0 byte, return nullptr ("") directly
		IRBranch(Reg8(9)),
	};
	block[6]->brTrue = block[7];
	block[6]->brFalse = block[8];

	block[7]->ins = {
		IRCall(EmptyOperand(), IDExtSymbol(size_t(BuiltinSymbol::free)), {RegPtr(0)}),
		IRReturn(ImmPtr(0)),
	};
	block[7]->brTrue = block[10];

	block[8]->ins = {
		IRStoreA(Imm8('\0'), RegPtr(0), RegPtr(1)),
		IR(RegPtr(10), Add, RegPtr(1), ImmPtr(1)),
		IRCall(RegPtr(0), IDExtSymbol(size_t(BuiltinSymbol::realloc)), {RegPtr(0), RegPtr(10)}),
		IRStore(Imm64(1), RegPtr(0)),	//TODO: Write Type ID
		IR(RegPtr(11), Sub, RegPtr(1), ImmPtr(objectHeader + stringHeader)),	//string size = current offset - 24
		IRStoreA(RegPtr(11), RegPtr(0), ImmPtr(objectHeader)),		//write string size
		IR(RegPtr(12), Add, RegPtr(0), ImmPtr(objectHeader)),
		IRReturn(RegPtr(12)),
	};
	block[8]->brTrue = block[10];

	block[9]->ins = {
		IR(Reg8(13), Seq, RegPtr(1), ImmPtr(objectHeader + stringHeader)),
		IRBranch(Reg8(13)),
	};
	block[9]->brTrue = block[1];		//if we read ' ' or '\n' and we didn't read any other char, we can continue the process
	block[9]->brFalse = block[8];

	Function ret;
	ret.inBlock = block[0];
	ret.outBlock = block[10];
	return ret;
}

Function MxBuiltin::builtin_getInt()
{
	Function ret;
	ret.inBlock = Block::construct();
	ret.outBlock = Block::construct();
	ret.inBlock->ins = {
		IR(RegPtr(0), Allocate, ImmPtr(4), ImmPtr(4)),
		IRCall(EmptyOperand(), IDExtSymbol(size_t(BuiltinSymbol::scanf)), {IDConst(size_t(BuiltinConst::Percent_d)), RegPtr(0)}),
		IR(Reg32(1), Load, RegPtr(0)),
		IRReturn(Reg32(1)),
	};
	ret.inBlock->brTrue = ret.outBlock;
	return ret;
}

Function MxBuiltin::builtin_toString()
{
	Function ret;
	ret.params.push_back(Reg32(0));
	ret.inBlock = Block::construct();
	ret.outBlock = Block::construct();
	ret.inBlock->ins = {
		IRCall(RegPtr(1), IDFunc(size_t(BuiltinFunc::newobject)), {ImmPtr(stringHeader+12), ImmPtr(0)}),	//TODO: change type id
		IR(RegPtr(2), Add, RegPtr(1), ImmPtr(stringHeader)),
		IRCall(EmptyOperand(), IDExtSymbol(size_t(BuiltinSymbol::snprintf)), {RegPtr(2), ImmPtr(12), IDConst(size_t(BuiltinConst::Percent_d)), Reg32(0)}),
		IRCall(RegPtr(3), IDExtSymbol(size_t(BuiltinSymbol::strlen)), {RegPtr(2)}),
		IRStore(RegPtr(3), RegPtr(1)),
		IRReturn(RegPtr(1)),
	};
	ret.inBlock->brTrue = ret.outBlock;
	return ret;
}

Function MxBuiltin::builtin_length()
{
	std::shared_ptr<Block> block[4];
	for (auto &blk : block)
		blk = Block::construct();

	block[0]->ins = {
		IR(Reg8(1), Seq, RegPtr(0), ImmPtr(0)),	//string == null
		IRBranch(Reg8(1), unlikely),
	};
	block[0]->brTrue = block[1];
	block[0]->brFalse = block[2];

	block[1]->ins = {
		IRReturn(Imm32(0)),
	};
	block[1]->brTrue = block[3];

	block[2]->ins = {
		IR(Reg32(2), Load, RegPtr(0)),
		IRReturn(Reg32(2)),
	};
	block[2]->brTrue = block[3];

	Function ret;
	ret.params.push_back(RegPtr(0));
	ret.inBlock = block[0];
	ret.outBlock = block[3];
	return ret;
}

Function MxBuiltin::builtin_substring()
{
	std::shared_ptr<Block> block[10];
	for (auto &blk : block)
		blk = Block::construct();

	//%0: pointer of the string
	//%1: left bound of substring, %2: right bound of substring

	block[0]->ins = {
		IR(Reg8(3), Sgt, Reg32(1), Reg32(2)),	//if left > right => return ""
		IRBranch(Reg8(3), unlikely),
	};
	block[0]->brTrue = block[1];
	block[0]->brFalse = block[2];

	block[1]->ins = {
		IRReturn(ImmPtr(0)),
	};
	block[1]->brTrue = block[9];

	block[2]->ins = {
		IR(Reg8(4), Seq, RegPtr(0), ImmPtr(0)),	//if string == null => return ""
		IRBranch(Reg8(4), unlikely),
	};
	block[2]->brTrue = block[1];
	block[2]->brFalse = block[3];

	block[3]->ins = {
		IR(RegPtr(14), Sext, Reg32(1)),
		IR(RegPtr(15), Sext, Reg32(2)),
		IR(Reg8(5), Slt, RegPtr(14), ImmPtr(0)),	//if left < 0 => left = 0
		IRBranch(Reg8(5), unlikely),
	};
	block[3]->brTrue = block[4];
	block[3]->brFalse = block[5];

	block[4]->ins = {
		IR(RegPtr(14), Move, ImmPtr(0)),
		IRJump(),
	};
	block[4]->brTrue = block[5];

	block[5]->ins = {
		IR(RegPtr(6), Load, RegPtr(0)),	//%6: length of string
		IR(Reg8(7), Sge, RegPtr(15), RegPtr(6)),	//if right >= length => right = length-1
		IRBranch(Reg8(7), unlikely),
	};
	block[5]->brTrue = block[6];
	block[5]->brFalse = block[7];

	block[6]->ins = {
		IR(RegPtr(15), Sub, RegPtr(6), ImmPtr(1)),
		IRJump(),
	};
	block[6]->brTrue = block[7];

	block[7]->ins = {
		IR(Reg8(8), Sgt, RegPtr(14), RegPtr(15)),	//if left > right => return ""
		IRBranch(Reg8(8), unlikely),
	};
	block[7]->brTrue = block[1];
	block[7]->brFalse = block[8];

	block[8]->ins = {
		IR(RegPtr(9), Sub, RegPtr(15), RegPtr(14)),	//%9: length of string (= right - left + 1)
		IR(RegPtr(9), Add, RegPtr(9), ImmPtr(1)),	
		IR(RegPtr(10), Add, RegPtr(9), ImmPtr(stringHeader + 1)),	//%10: size of string (=length + stringheader + 1 ('\0'))
		IRCall(RegPtr(11), IDFunc(size_t(BuiltinFunc::newobject)), {RegPtr(10), ImmPtr(0)}),	//TODO: Type ID	
		IR(RegPtr(12), Add, RegPtr(0), ImmPtr(stringHeader)),
		IR(RegPtr(12), Add, RegPtr(12), RegPtr(14)),				//%12: start ptr of src
		IR(RegPtr(13), Add, RegPtr(11), ImmPtr(stringHeader)),	//%13: start ptr of dst
		IRCall(EmptyOperand(), IDExtSymbol(size_t(BuiltinSymbol::memcpy)), {RegPtr(13), RegPtr(12), RegPtr(9)}),
		IRStoreA(Imm8(0), RegPtr(13), RegPtr(9)),		//write '\0'
		IRStore(RegPtr(9), RegPtr(11)),		//write string size
		IRReturn(RegPtr(11)),
	};
	block[8]->brTrue = block[9];

	Function ret;
	ret.params = { RegPtr(0), Reg32(1), Reg32(2) };
	ret.inBlock = block[0];
	ret.outBlock = block[9];
	return ret;
}

Function MxBuiltin::builtin_parseInt()
{
	std::shared_ptr<Block> block[4];
	for (auto &blk : block)
		blk = Block::construct();

	block[0]->ins = {
		IR(RegPtr(2), Allocate, ImmPtr(4), ImmPtr(4)),
		IR(Reg8(1), Seq, RegPtr(0), ImmPtr(0)),
		IRBranch(Reg8(1), unlikely),
	};
	block[0]->brTrue = block[1];
	block[0]->brFalse = block[2];

	block[1]->ins = {
		IRReturn(Imm32(0)),
	};
	block[1]->brTrue = block[3];

	block[2]->ins = {
		IR(RegPtr(3), Add, RegPtr(0), ImmPtr(stringHeader)),
		IRCall(EmptyOperand(), IDExtSymbol(size_t(BuiltinSymbol::sscanf)), {RegPtr(3), IDConst(size_t(BuiltinConst::Percent_d)), RegPtr(2)}),
		IR(Reg32(4), Load, RegPtr(2)),
		IRReturn(Reg32(4)),
	};
	block[2]->brTrue = block[3];

	Function ret;
	ret.params = { RegPtr(0) };
	ret.inBlock = block[0];
	ret.outBlock = block[3];
	return ret;
}

Function MxBuiltin::builtin_ord_safe()
{
	std::shared_ptr<Block> block[6];
	for (auto &blk : block)
		blk = Block::construct();

	//%0: pointer of the string
	//%1: index of the target character

	block[0]->ins = {
		IR(Reg8(2), Seq, RegPtr(0), ImmPtr(0)),	//if %0 is empty string => ERROR
		IRBranch(Reg8(2), unlikely),
	};
	block[0]->brTrue = block[1];
	block[0]->brFalse = block[2];

	block[1]->ins = {
		IRCall(EmptyOperand(), IDFunc(size_t(BuiltinFunc::runtime_error)), {IDConst(size_t(BuiltinConst::subscript_out_of_range))}),
		IRReturn(Imm32(0)),
	};
	block[1]->brTrue = block[5];

	block[2]->ins = {
		IR(Reg8(3), Slt, Reg32(1), Imm32(0)),	//if idx < 0 => ERROR
		IRBranch(Reg8(3), unlikely),
	};
	block[2]->brTrue = block[1];
	block[2]->brFalse = block[3];

	block[3]->ins = {
		IR(RegPtr(8), Zext, Reg32(1)),
		IR(RegPtr(4), Load, RegPtr(0)),			//%4: length of string
		IR(Reg8(5), Sge, RegPtr(8), RegPtr(4)),	//if idx >= length => ERROR
		IRBranch(Reg8(5), unlikely),
	};
	block[3]->brTrue = block[1];
	block[3]->brFalse = block[4];

	block[4]->ins = {
		IR(RegPtr(6), Add, RegPtr(0), ImmPtr(stringHeader)),
		IR(Reg8(7), LoadA, RegPtr(6), RegPtr(8)),
		IR(Reg32(9), Zext, Reg8(7)),
		IRReturn(Reg32(9)),
	};
	block[4]->brTrue = block[5];

	Function ret;
	ret.params = { RegPtr(0), Reg32(1) };
	ret.inBlock = block[0];
	ret.outBlock = block[5];
	return ret;
}

Function MxBuiltin::builtin_ord_unsafe()
{
	Function ret;
	ret.inBlock = Block::construct();
	ret.outBlock = Block::construct();
	ret.params = { RegPtr(0), Reg32(1) };
	ret.inBlock->ins = {
		IR(RegPtr(2), Add, RegPtr(0), ImmPtr(stringHeader)),
		IR(RegPtr(3), Sext, Reg32(1)),
		IR(Reg8(4), LoadA, RegPtr(2), RegPtr(3)),
		IR(Reg32(4), Zext, Reg8(4)),
		IRReturn(Reg32(4)),
	};
	ret.inBlock->brTrue = ret.outBlock;
	return ret;
}

Function MxBuiltin::builtin_size()
{
	std::shared_ptr<Block> block[4];
	for (auto &blk : block)
		blk = Block::construct();

	//%0: pointer of the array

	block[0]->ins = {
		IR(Reg8(1), Seq, RegPtr(0), ImmPtr(0)),	//if array == null => ERROR
		IRBranch(Reg8(1), unlikely),
	};
	block[0]->brTrue = block[1];
	block[0]->brFalse = block[2];

	block[1]->ins = {
		IRCall(EmptyOperand(), IDFunc(size_t(BuiltinFunc::runtime_error)), {IDConst(size_t(BuiltinConst::null_ptr))}),
		IRReturn(Imm32(0)),
	};
	block[1]->brTrue = block[3];

	block[2]->ins = {
		IR(RegPtr(2), Load, RegPtr(0)),
		IRReturn(Reg32(2)),
	};
	block[2]->brTrue = block[3];

	Function ret;
	ret.params = { RegPtr(0) };
	ret.inBlock = block[0];
	ret.outBlock = block[3];
	return ret;
}

Function MxBuiltin::builtin_size_unsafe()
{
	Function ret;
	ret.params = { RegPtr(0) };
	ret.inBlock = Block::construct();
	ret.outBlock = Block::construct();
	ret.inBlock->ins = {
		IR(RegPtr(1), Load, RegPtr(0)),
		IRReturn(Reg32(1)),
	};
	ret.inBlock->brTrue = ret.outBlock;
	return ret;
}

Function MxBuiltin::builtin_runtime_error()
{
	Function ret;
	ret.params = { RegPtr(0) };
	ret.inBlock = Block::construct();
	ret.outBlock = Block::construct();
	ret.inBlock->ins = {
		IR(RegPtr(1), Load, IDExtSymbol(size_t(BuiltinSymbol::Stderr))),
		IRCall(EmptyOperand(), IDExtSymbol(size_t(BuiltinSymbol::fputs)), {IDConst(size_t(BuiltinConst::runtime_error)), RegPtr(1)}),
		IRCall(EmptyOperand(), IDExtSymbol(size_t(BuiltinSymbol::fputs)), {RegPtr(0), RegPtr(1)}),
		IRCall(EmptyOperand(), IDExtSymbol(size_t(BuiltinSymbol::fputs)), {IDConst(size_t(BuiltinConst::line_break)), RegPtr(1)}),
		IRCall(EmptyOperand(), IDExtSymbol(size_t(BuiltinSymbol::exit)), {Imm32(-1)}),
		IRReturn(),
	};
	ret.inBlock->brTrue = ret.outBlock;
	return ret;
}

Function MxBuiltin::builtin_strcat()
{
	std::shared_ptr<Block> block[8];
	for (auto &blk : block)
		blk = Block::construct();

	//%0: pointer of left string
	//%1: pointer of right string

	block[0]->ins = {
		IR(Reg8(2), Seq, RegPtr(0), ImmPtr(0)),
		IRBranch(Reg8(2), unlikely),
	};
	block[0]->brTrue = block[1];
	block[0]->brFalse = block[4];

	block[1]->ins = {
		IR(Reg8(3), Seq, RegPtr(1), ImmPtr(0)),
		IRBranch(Reg8(3), unlikely),
	};
	block[1]->brTrue = block[2];
	block[1]->brFalse = block[3];

	block[2]->ins = {			//str1 == null && str2 == null
		IRReturn(ImmPtr(0)),
	};
	block[2]->brTrue = block[7];

	block[3]->ins = {			//str1 == null && str2 != null
		IR(RegPtr(4), Load, RegPtr(1)),
		IR(RegPtr(4), Add, RegPtr(4), ImmPtr(stringHeader + 1)),	//%4: size of string (=length + header + 1)
		IRCall(RegPtr(5), IDFunc(size_t(BuiltinFunc::newobject)), {RegPtr(4), ImmPtr(0)}),	//TODO: string type id
		IRCall(EmptyOperand(), IDExtSymbol(size_t(BuiltinSymbol::memcpy)), {RegPtr(5), RegPtr(1), RegPtr(4)}),
		IRReturn(RegPtr(5)),
	};
	block[3]->brTrue = block[7];

	block[4]->ins = {
		IR(Reg8(6), Seq, RegPtr(1), ImmPtr(0)),
		IRBranch(Reg8(6), unlikely),
	};
	block[4]->brTrue = block[5];
	block[4]->brFalse = block[6];

	block[5]->ins = {		//str1 != null && str2 == null
		IR(RegPtr(7), Move, RegPtr(1)),
		IR(RegPtr(1), Move, RegPtr(0)),
		IR(RegPtr(0), Move, RegPtr(7)),
		IRJump(),
	};
	block[5]->brTrue = block[3];

	block[6]->ins = {		//str1 != null && str2 != null
		IR(RegPtr(8), Load, RegPtr(0)),	//%8: length of str1
		IR(RegPtr(9), Load, RegPtr(1)),	//%9: length of str2
		IR(RegPtr(10), Add, RegPtr(8), RegPtr(9)),		//%10: length of target string (len(str1) + len(str2))
		IR(RegPtr(11), Add, RegPtr(10), ImmPtr(stringHeader + 1)),	//%11: size of target string (len(str1) + len(str2) + stringheader + 1)
		IRCall(RegPtr(12), IDFunc(size_t(BuiltinFunc::newobject)), {RegPtr(11), ImmPtr(0)}),	//TODO: Type ID
		IR(RegPtr(13), Add, RegPtr(12), ImmPtr(stringHeader)),	//%13: start ptr of target string
		IR(RegPtr(14), Add, RegPtr(0), ImmPtr(stringHeader)),	//%14: start ptr of str1
		IR(RegPtr(15), Add, RegPtr(1), ImmPtr(stringHeader)),	//%15: start ptr of str2
		IRCall(EmptyOperand(), IDExtSymbol(size_t(BuiltinSymbol::memcpy)), {RegPtr(13), RegPtr(14), RegPtr(8)}),
		IR(RegPtr(13), Add, RegPtr(13), RegPtr(8)),
		IRCall(EmptyOperand(), IDExtSymbol(size_t(BuiltinSymbol::memcpy)), {RegPtr(13), RegPtr(15), RegPtr(9)}),
		IRStoreA(Imm8(0), RegPtr(13), RegPtr(9)),
		IRStore(RegPtr(10), RegPtr(12)),
		IRReturn(RegPtr(12)),
	};
	block[6]->brTrue = block[7];

	Function ret;
	ret.params = { RegPtr(0), RegPtr(1) };
	ret.inBlock = block[0];
	ret.outBlock = block[7];
	return ret;
}

Function MxBuiltin::builtin_strcmp()
{
	std::shared_ptr<Block> block[8];
	for (auto &blk : block)
		blk = Block::construct();

	block[0]->ins = {
		IR(Reg8(2), Seq, RegPtr(0), ImmPtr(0)),
		IRBranch(Reg8(2), unlikely),
	};
	block[0]->brTrue = block[1];
	block[0]->brFalse = block[4];

	block[1]->ins = {
		IR(Reg8(3), Seq, RegPtr(1), ImmPtr(0)),
		IRBranch(Reg8(3), unlikely),
	};
	block[1]->brTrue = block[2];
	block[1]->brFalse = block[3];

	block[2]->ins = {		//str1 == null && str2 == null
		IRReturn(Imm32(0)),
	};
	block[2]->brTrue = block[7];

	block[3]->ins = {		//str1 == null && str2 != null
		IRReturn(Imm32(-1)),
	};
	block[3]->brTrue = block[7];

	block[4]->ins = {
		IR(Reg8(4), Seq, RegPtr(1), ImmPtr(0)),
		IRBranch(Reg8(4), unlikely),
	};
	block[4]->brTrue = block[5];
	block[4]->brFalse = block[6];

	block[5]->ins = {		//str1 != null && str2 == null
		IRReturn(Imm32(1)),
	};
	block[5]->brTrue = block[7];

	block[6]->ins = {		//str1 != null && str2 != null
		IR(RegPtr(5), Add, RegPtr(0), ImmPtr(stringHeader)),
		IR(RegPtr(6), Add, RegPtr(1), ImmPtr(stringHeader)),
		IRCall(Reg32(7), IDExtSymbol(size_t(BuiltinSymbol::strcmp)), {RegPtr(5), RegPtr(6)}),
		IRReturn(Reg32(7)),
	};
	block[6]->brTrue = block[7];

	Function ret;
	ret.params = { RegPtr(0), RegPtr(1) };
	ret.inBlock = block[0];
	ret.outBlock = block[7];
	return ret;
}

Function MxBuiltin::builtin_subscript_safe(size_t size)
{
	assert(size == 1 || size == 2 || size == 4 || size == 8);

	std::shared_ptr<Block> block[6];
	for (auto &blk : block)
		blk = Block::construct();

	block[0]->ins = {
		IR(Reg8(2), Seq, RegPtr(0), ImmPtr(0)),
		IRBranch(Reg8(2), unlikely),
	};
	block[0]->brTrue = block[1];
	block[0]->brFalse = block[2];

	block[1]->ins = {	//array == null
		IRCall(EmptyOperand(), IDFunc(size_t(BuiltinFunc::runtime_error)), {IDConst(size_t(BuiltinConst::null_ptr))}),
		IRReturn(ImmPtr(0)),
	};
	block[1]->brTrue = block[5];

	block[2]->ins = {
		IR(RegPtr(3), Load, RegPtr(0)),
		IR(RegPtr(8), Sext, Reg32(1)),
		IR(Reg8(4), Sgeu, RegPtr(8), RegPtr(3)),
		IRBranch(Reg8(4), unlikely),
	};
	block[2]->brTrue = block[3];
	block[2]->brFalse = block[4];

	block[3]->ins = {	//idx < 0 || idx >= size(array)
		IRCall(EmptyOperand(), IDFunc(size_t(BuiltinFunc::runtime_error)), {IDConst(size_t(BuiltinConst::subscript_out_of_range))}),
		IRReturn(ImmPtr(0)),
	};
	block[3]->brTrue = block[5];

	block[4]->ins = {
		IR(RegPtr(5), Add, RegPtr(0), ImmPtr(arrayHeader)),
		IR(RegPtr(6), Mult, RegPtr(8), ImmPtr(size)),
		IR(RegPtr(7), Add, RegPtr(5), RegPtr(6)),
		//IR(Reg(7), LoadA, RegPtr(5), RegPtr(6)),
		IRReturn(RegPtr(7)),
	};
	block[4]->brTrue = block[5];

	Function ret;
	ret.params = { RegPtr(0), Reg32(1) };
	ret.inBlock = block[0];
	ret.outBlock = block[5];
	return ret;
}

Function MxBuiltin::builtin_subscript_unsafe(size_t size)
{
	Function ret;
	ret.params = { RegPtr(0), Reg32(1) };
	ret.inBlock = Block::construct();
	ret.outBlock = Block::construct();
	ret.inBlock->ins = {
		IR(RegPtr(2), Add, RegPtr(0), ImmPtr(arrayHeader)),
		IR(RegPtr(3), Sext, Reg32(1)),
		IR(RegPtr(4), Mult, RegPtr(3), ImmPtr(size)),
		IR(RegPtr(5), Add, RegPtr(2), RegPtr(4)),
		IRReturn(RegPtr(5)),
	};
	ret.inBlock->brTrue = ret.outBlock;
	return ret;
}

Function MxBuiltin::builtin_newobject()
{
	std::shared_ptr<Block> block[4];
	for (auto &blk : block)
		blk = Block::construct();

	block[0]->ins = {
		IR(RegPtr(0), Add, RegPtr(0), ImmPtr(objectHeader)),		//actual size = object size + object header
		IRCall(RegPtr(2), IDExtSymbol(size_t(BuiltinSymbol::malloc)), {RegPtr(0)}),
		IR(Reg8(3), Seq, RegPtr(2), ImmPtr(0)),
		IRBranch(Reg8(3), unlikely),
	};
	block[0]->brTrue = block[1];
	block[0]->brFalse = block[2];

	block[1]->ins = {
		IRCall(EmptyOperand(), IDFunc(size_t(BuiltinFunc::runtime_error)), {IDConst(size_t(BuiltinConst::bad_allocation))}),
		IRReturn(ImmPtr(0)),
	};
	block[1]->brTrue = block[3];

	block[2]->ins = {
		IRStore(ImmPtr(1), RegPtr(2)),	//reference count
		IRStoreA(RegPtr(1), RegPtr(2), ImmPtr(POINTER_SIZE)),	//type id
		IR(RegPtr(4), Add, RegPtr(2), ImmPtr(objectHeader)),	//return the address of the object (header not included)
		IRReturn(RegPtr(4)),
		// ********  ********  **......
		// ^refcount ^typeid   ^actual return address
	};
	block[2]->brTrue = block[3];

	Function ret;
	ret.params = { RegPtr(0), RegPtr(1) };
	ret.inBlock = block[0];
	ret.outBlock = block[3];
	return ret;
}

MxIR::Function MxBuiltin::builtin_newobject_zero()
{
	std::shared_ptr<Block> block[4];
	for (auto &blk : block)
		blk = Block::construct();

	block[0]->ins = {
		IRCall(RegPtr(2), IDFunc(size_t(BuiltinFunc::newobject)), {RegPtr(0), RegPtr(1)}),
		IR(Reg8(3), Seq, RegPtr(2), ImmPtr(0)),
		IRBranch(Reg8(3), unlikely),
	};
	block[0]->brTrue = block[1];
	block[0]->brFalse = block[2];

	block[1]->ins = {
		IRReturn(ImmPtr(0)),
	};
	block[1]->brTrue = block[3];

	block[2]->ins = {
		IRCall(EmptyOperand(), IDExtSymbol(size_t(BuiltinSymbol::memset)), {RegPtr(2), Imm32(0), RegPtr(0)}),
		IRReturn(RegPtr(2)),
	};
	block[2]->brTrue = block[3];

	Function ret;
	ret.params = { RegPtr(0), RegPtr(1) };
	ret.inBlock = block[0];
	ret.outBlock = block[3];
	return ret;
}

MxIR::Function MxBuiltin::builtin_addref_object()
{
	std::shared_ptr<Block> block[5];
	for (auto &blk : block)
		blk = Block::construct();

	block[0]->ins = {
		IR(Reg8(4), Sne, RegPtr(0), ImmPtr(0)),
		IRBranch(Reg8(4), likely),
	};
	block[0]->brTrue = block[1];
	block[0]->brFalse = block[3];

	block[1]->ins = {
		IR(RegPtr(1), LoadA, RegPtr(0), ImmPtr(-std::int64_t(objectHeader))),
		IR(Reg8(2), Sne, RegPtr(1), ImmPtr(-1)),
		IRBranch(Reg8(2)),
	};
	block[1]->brTrue = block[2];
	block[1]->brFalse = block[3];

	block[2]->ins = {
		IR(RegPtr(3), Add, RegPtr(1), ImmPtr(1)),
		IRStoreA(RegPtr(3), RegPtr(0), ImmPtr(-std::int64_t(objectHeader))),
		IRReturn(),
	};
	block[2]->brTrue = block[4];

	block[3]->ins = {
		IRReturn(),
	};
	block[3]->brTrue = block[4];

	Function ret;
	ret.inBlock = block[0];
	ret.outBlock = block[4];
	ret.params = { RegPtr(0) };
	return ret;
}

MxIR::Function MxBuiltin::builtin_release_string()
{
	std::shared_ptr<Block> block[6];
	for (auto &blk : block)
		blk = Block::construct();

	block[0]->ins = {
		IR(Reg8(6), Sne, RegPtr(0), ImmPtr(0)),
		IRBranch(Reg8(6), likely),
	};
	block[0]->brTrue = block[1];
	block[0]->brFalse = block[4];
	
	block[1]->ins = {
		IR(RegPtr(1), Sub, RegPtr(0), ImmPtr(objectHeader)),
		IR(RegPtr(2), Load, RegPtr(1)),
		IR(Reg8(3), Sne, RegPtr(2), ImmPtr(-1)),
		IRBranch(Reg8(3)),
	};
	block[1]->brTrue = block[2];
	block[1]->brFalse = block[4];

	block[2]->ins = {
		IR(RegPtr(4), Sub, RegPtr(2), ImmPtr(1)),
		IRStore(RegPtr(4), RegPtr(1)),
		IR(Reg8(5), Seq, RegPtr(4), ImmPtr(0)),
		IRBranch(Reg8(5)),
	};
	block[2]->brTrue = block[3];
	block[2]->brFalse = block[4];

	block[3]->ins = {
		IRCall(EmptyOperand(), IDExtSymbol(size_t(BuiltinSymbol::free)), {RegPtr(1)}),
		IRReturn(),
	};
	block[3]->brTrue = block[5];

	block[4]->ins = {
		IRReturn(),
	};
	block[4]->brTrue = block[5];

	Function ret;
	ret.inBlock = block[0];
	ret.outBlock = block[5];
	ret.params = { RegPtr(0) };
	return ret;
}

MxIR::Function MxBuiltin::builtin_release_array(bool internal)
{
	std::shared_ptr<Block> block[10];
	for (auto &blk : block)
		blk = Block::construct();

	block[7]->ins = {
		IR(Reg8(9), Sne, RegPtr(0), ImmPtr(0)),
		IRBranch(Reg8(9)),
	};
	block[7]->brTrue = block[8];
	block[7]->brFalse = block[9];

	block[8]->ins = {
		IR(RegPtr(10), LoadA, RegPtr(0), ImmPtr(-std::int64_t(objectHeader))),
		IR(RegPtr(10), Sub, RegPtr(10), ImmPtr(1)),
		IRStoreA(RegPtr(10), RegPtr(0), ImmPtr(-std::int64_t(objectHeader))),
		IR(Reg8(11), Seq, RegPtr(10), ImmPtr(0)),
		IRBranch(Reg8(11)),
	};
	block[8]->brTrue = block[0];
	block[8]->brFalse = block[9];

	block[9]->ins = {
		IRReturn(),
	};
	block[9]->brTrue = block[6];

	block[0]->ins = {
		IR(Reg8(2), Sle, Reg32(1), Imm32(internal ? 1 : 0)),
		IRBranch(Reg8(2)),
	};
	block[0]->brTrue = internal ? block[5] : block[1];
	block[0]->brFalse = block[2];

	block[1]->ins = {
		IRCall(EmptyOperand(), IDFunc(size_t(BuiltinFunc::release_object)), {RegPtr(0)}),
		IRReturn(),
	};
	block[1]->brTrue = block[6];

	block[2]->ins = {
		IR(Reg32(7), Sub, Reg32(1), Imm32(1)),
		IR(RegPtr(3), Load, RegPtr(0)),
		IR(RegPtr(3), Shl, RegPtr(3), ImmPtr(3)),
		IR(RegPtr(3), Add, RegPtr(3), ImmPtr(8)),
		IR(RegPtr(3), Add, RegPtr(3), RegPtr(0)),
		IR(RegPtr(4), Add, RegPtr(0), ImmPtr(8)),
		IRJump(),
	};
	block[2]->brTrue = block[3];

	block[3]->ins = {
		IR(Reg8(5), Seq, RegPtr(4), RegPtr(3)),
		IRBranch(Reg8(5), unlikely),
	};
	block[3]->brTrue = block[5];
	block[3]->brFalse = block[4];

	block[4]->ins = {
		IR(RegPtr(6), Load, RegPtr(4)),
		IRCall(EmptyOperand(), IDFunc(size_t(internal ? BuiltinFunc::release_array_internal : BuiltinFunc::release_array_object)), {RegPtr(6), Reg32(7)}),
		IR(RegPtr(4), Add, RegPtr(4), ImmPtr(8)),
		IRJump(),
	};
	block[4]->brTrue = block[3];

	block[5]->ins = {
		IR(RegPtr(8), Sub, RegPtr(0), ImmPtr(objectHeader)),
		IRCall(EmptyOperand(), IDExtSymbol(size_t(BuiltinSymbol::free)), {RegPtr(8)}),
		IRReturn(),
	};
	block[5]->brTrue = block[6];

	Function ret;
	ret.inBlock = block[7];
	ret.outBlock = block[6];
	ret.params = { RegPtr(0), Reg32(1) };
	return ret;
}

MxIR::Function MxBuiltin::builtin_stub(const std::vector<Operand> &param)
{
	Function ret;
	ret.inBlock = Block::construct();
	ret.outBlock = Block::construct();
	ret.inBlock->ins = { IRReturn() };
	ret.inBlock->brTrue = ret.outBlock;
	ret.params = param;
	return ret;
}