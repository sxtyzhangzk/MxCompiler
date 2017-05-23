#ifndef MX_COMPILER_UTILS_JOIN_ITERATOR
#define MX_COMPILER_UTILS_JOIN_ITERATOR

#include <utility>

template<typename RetType, typename ...Container>
class JoinWrapper;
template<typename RetType, typename Container1, typename Container2>
class JoinWrapperPair;
template<typename RetType, typename Container1, typename Container2>
class JoinIterator;
template<typename RetType, typename ...Container>
JoinWrapper<RetType, Container...> join(Container&& ...containers);

template<typename RetType, typename ...Container>
class JoinWrapper
{
};

template<typename RetType, typename Container>
class JoinWrapper<RetType, Container>
{
	template<typename RT, typename ...C>
	friend JoinWrapper<RT, C...> join(C&& ...);
	template<typename RT, typename ...C>
	friend class JoinWrapper;

	Container container;
public:
	typedef decltype(container.begin()) iterator;
	iterator begin() { return container.begin(); }
	iterator end() { return container.end(); }

private:
	JoinWrapper(Container &&container) : container(container) {}
};

template<typename RetType, typename Container1, typename ...ContainerOther>
class JoinWrapper<RetType, Container1, ContainerOther...>
{
	template<typename RT, typename ...C>
	friend JoinWrapper<RT, C...> join(C&& ...);
	template<typename RT, typename ...C>
	friend class JoinWrapper;
public:
	typedef JoinIterator<RetType, Container1, JoinWrapper<RetType, ContainerOther...>> iterator;
	iterator begin() { return wrapper.begin(); }
	iterator end() { return wrapper.end(); }

private:
	JoinWrapper(Container1 &&container1, ContainerOther &&...other) : wrapper(std::forward<Container1>(container1), join<RetType>(std::forward<ContainerOther>(other)...)) {}
private:
	JoinWrapperPair<RetType, Container1, JoinWrapper<RetType, ContainerOther...>> wrapper;
};


template<typename RetType, typename Container1, typename Container2>
class JoinWrapperPair
{
	friend class JoinIterator<RetType, Container1, Container2>;
	template<typename RT, typename ...C>
	friend class JoinWrapper;
public:
	JoinIterator<RetType, Container1, Container2> begin();
	JoinIterator<RetType, Container1, Container2> end();

private:
	JoinWrapperPair(Container1 &&container1, Container2 &&container2) : container1(std::forward<Container1>(container1)), container2(std::forward<Container2>(container2)) {}
private:
	Container1 container1;
	Container2 container2;
};

template<typename RetType, typename Container1, typename Container2>
class JoinIterator
{
	friend class JoinWrapperPair<RetType, Container1, Container2>;
	typedef JoinIterator<RetType, Container1, Container2> ThisType;
public:
	ThisType & operator++()
	{
		if (!wrapper)
			return *this;
		if (inContainer1)
		{
			++iter1;
			if (iter1 == wrapper->container1.end())
			{
				inContainer1 = false;
				iter2 = wrapper->container2.begin();
			}
		}
		else
			++iter2;
		return *this;
	}
	ThisType & operator--()
	{
		if (!wrapper)
			return *this;
		if (!inContainer1)
		{
			if (iter2 == wrapper->container2.begin())
			{
				inContainer1 = true;
				iter1 = --wrapper->container1.end();
			}
		}
		else
			--iter1;
		return *this;
	}
	ThisType operator++(int)
	{
		ThisType ret = *this;
		++(*this);
		return ret;
	}
	ThisType operator--(int)
	{
		ThisType ret = *this;
		--(*this);
		return ret;
	}
	bool operator==(const ThisType &rhs) const
	{
		if (inContainer1 != rhs.inContainer1)
			return false;
		if (inContainer1)
			return iter1 == rhs.iter1;
		return iter2 == rhs.iter2;
	}
	bool operator!=(const ThisType &rhs) const
	{
		return !((*this) == rhs);
	}

	RetType & operator*()
	{
		if (inContainer1)
			return static_cast<RetType &>(*iter1);
		return static_cast<RetType &>(*iter2);
	}
	RetType * operator->()
	{
		if (inContainer1)
			return static_cast<RetType *>(&(*iter1));
		return static_cast<RetType *>(&(*iter2));
	}

private:
	JoinWrapperPair<RetType, Container1, Container2> *wrapper;
	decltype(((typename std::remove_reference<Container1>::type *)nullptr)->begin()) iter1;
	decltype(((typename std::remove_reference<Container2>::type *)nullptr)->begin()) iter2;
	bool inContainer1;
};

template<typename RetType, typename Container1, typename Container2>
JoinIterator<RetType, Container1, Container2> JoinWrapperPair<RetType, Container1, Container2>::begin()
{
	JoinIterator<RetType, Container1, Container2> iter;
	iter.wrapper = this;
	if (container1.begin() == container1.end())
	{
		iter.iter2 = container2.begin();
		iter.inContainer1 = false;
	}
	else
	{
		iter.iter1 = container1.begin();
		iter.inContainer1 = true;
	}
	return iter;
}

template<typename RetType, typename Container1, typename Container2>
JoinIterator<RetType, Container1, Container2> JoinWrapperPair<RetType, Container1, Container2>::end()
{
	JoinIterator<RetType, Container1, Container2> iter;
	iter.wrapper = this;
	iter.iter2 = container2.end();
	iter.inContainer1 = false;
	return iter;
}

template<typename RetType, typename ...Container>
JoinWrapper<RetType, Container...> join(Container&& ...containers)
{
	return JoinWrapper<RetType, Container...>(std::forward<Container>(containers)...);
}

#endif