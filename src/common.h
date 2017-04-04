#ifndef MX_COMPILER_COMMON_H
#define MX_COMPILER_COMMON_H

#ifdef _DEBUG
#define IF_DEBUG(x) x
#include <cassert>
#else
#define IF_DEBUG(x)
#define assert(expr) ((void)0)
#endif

#include <antlr4-common.h>
#include <string>
#include <vector>

constexpr size_t MAX_ERROR = 100;

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
};


#endif