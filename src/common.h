#ifndef MX_COMPILER_COMMON_H
#define MX_COMPILER_COMMON_H

#if defined(_DEBUG) && !defined(NDEBUG)
#define IF_DEBUG(x) x
#else
#define IF_DEBUG(x)
#endif

#include "common_headers.h"

constexpr size_t MAX_ERROR = 100;
constexpr size_t MAX_STRINGSIZE = 100000;
constexpr size_t MAX_STRINGMEMUSAGE = 10000000;
constexpr size_t POINTER_SIZE = 8;

enum ValueType
{
	lvalue, 
	xvalue, //expiring value
	rvalue	//rvalue or constant
};

struct MxType
{
	enum type
	{
		Void, Bool, Integer, String, Object, Function
	};
	type mainType;
	size_t arrayDim;	//0 for non-array
	size_t className;	//-1 for undetermined object (null)
	size_t funcOLID;
	IF_DEBUG(std::string strClassName);

	bool isNull() const
	{
		return mainType == Object && className == size_t(-1);
	}
	bool isObject() const
	{
		return mainType == Object || mainType == String || arrayDim > 0;
	}
	size_t getSize() const
	{
		if (arrayDim > 0)
			return POINTER_SIZE;
		if (mainType == Void)
			return 0;
		if (mainType == Bool)
			return 1;
		if (mainType == Integer)
			return 4;
		return POINTER_SIZE;
	}
	bool operator==(const MxType &rhs) const
	{
		if (rhs.isNull())
			return rhs == *this;
		if (isNull() && (rhs.mainType == Object || rhs.arrayDim > 0))
			return true;
		if (mainType != rhs.mainType || arrayDim != rhs.arrayDim)
			return false;
		if (mainType == Object)
			return className == rhs.className;
		if (mainType == Function)
			return funcOLID == rhs.funcOLID;
		return true;
	}
	bool operator!=(const MxType &rhs) const
	{
		return !(*this == rhs);
	}

	static MxType Null()
	{
		return MxType{ Object, 0, size_t(-1) };
	}
};

class CompileFlags
{
public:
	bool disable_access_protect = false;
	bool optim_register_allocation = false;
	bool optim_inline = false;
	int inline_param = 1000;

	static CompileFlags * getInstance()
	{
		static CompileFlags instance;
		return &instance;
	}
private:
	CompileFlags() : disable_access_protect(false) {}
	CompileFlags(const CompileFlags &other) = delete;
};

inline std::string transferHTML(const std::string &in)
{
	std::string ret;
	for (char c : in)
	{
		if (c == '<')
			ret += "&lt;";
		else if (c == '>')
			ret += "&gt;";
		else if (c == ' ')
			ret += "&nbsp;";
		else if (c == '"')
			ret += "&quot;";
		else if (c == '&')
			ret += "&amp;";
		else if (c == '\n')
			ret += "<br align='left'/>";
		else
			ret += c;
	}
	return ret;
}

//align addr to align bytes
constexpr std::uint64_t alignAddr(std::uint64_t addr, std::uint64_t align)
{
	return (addr + align - 1) / align * align;
}

inline void prints(std::ostream &out) {}

template<typename Tnow, typename ...T>
void prints(std::ostream &out, Tnow &&now, T&&... val)
{
	out << std::forward<Tnow>(now);
	prints(out, std::forward<T>(val)...);
}



#endif