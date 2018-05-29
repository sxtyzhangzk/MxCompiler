#ifndef MX_COMPILER_UTILS_DISPATCH_LENGTH_H
#define MX_COMPILER_UTILS_DISPATCH_LENGTH_H

#include "../common.h"

template<typename F>
auto dispatch_length(F &&f, uint64_t value, size_t length, bool sign)
{
	if (!sign)
	{
		if (length == 1)
			return f((uint8_t)value);
		else if (length == 2)
			return f((uint16_t)value);
		else if (length == 4)
			return f((uint32_t)value);
		else
		{
			assert(length == 8);
			return f((uint64_t)value);
		}
	}
	else
	{
		if (length == 1)
			return f((int8_t)value);
		else if (length == 2)
			return f((int16_t)value);
		else if (length == 4)
			return f((int32_t)value);
		else
		{
			assert(length == 8);
			return f((int64_t)value);
		}
	}
}

template<typename F, typename ...T>
auto dispatch_length(F &&f, uint64_t value, size_t length, bool sign, T &&...other)
{
	return dispatch_length([&](auto ...valN)
	{
		return dispatch_length([&](auto val1)
		{
			return f(val1, valN...);
		}, value, length, sign);
	}, other...);
}

#endif