#ifndef MX_COMPILER_UTILS_ELEMENT_ADAPTER_H
#define MX_COMPILER_UTILS_ELEMENT_ADAPTER_H

#include <utility>

template<typename Container, typename AdapterFunc>
class ElementAdapter;
template<typename Container, typename AdapterFunc>
ElementAdapter<Container, AdapterFunc> element_adapter(Container &&container, AdapterFunc adapter);

template<typename Container, typename AdapterFunc>
class ElementAdapter
{
	friend ElementAdapter<Container, AdapterFunc> element_adapter<Container, AdapterFunc>(Container &&container, AdapterFunc adapter);
	//typedef decltype(std::declval<AdapterFunc>()(*(((typename std::remove_reference<Container>::type *)nullptr)->begin()))) DstType;
public:
	class iterator
	{
		friend class ElementAdapter<Container, AdapterFunc>;
	public:
		iterator & operator++()
		{
			++iter;
			return *this;
		}
		iterator & operator--()
		{
			--iter;
			return *this;
		}
		iterator operator++(int)
		{
			iterator ret = *this;
			++(*this);
			return ret;
		}
		iterator operator--(int)
		{
			iterator ret = *this;
			--(*this);
			return ret;
		}
		bool operator==(const iterator &rhs) const
		{
			return iter == rhs.iter;
		}
		bool operator!=(const iterator &rhs) const
		{
			return iter != rhs.iter;
		}
		auto & operator*()
		{
			return adapter->adapter(*iter);
		}
		auto * operator->()
		{
			return &adapter->adapter(*iter);
		}
	private:
		decltype(((typename std::remove_reference<Container>::type *)nullptr)->begin()) iter;
		ElementAdapter *adapter;
	};
public:

	iterator begin()
	{
		iterator iter;
		iter.iter = container.begin();
		iter.adapter = this;
		return iter;
	}
	iterator end()
	{
		iterator iter;
		iter.iter = container.end();
		iter.adapter = this;
		return iter;
	}

private:
	ElementAdapter(Container &&container, AdapterFunc adapter) : container(std::forward<Container>(container)), adapter(adapter) {}

private:
	Container container;
	AdapterFunc adapter;
};

template<typename Container, typename AdapterFunc>
ElementAdapter<Container, AdapterFunc> element_adapter(Container &&container, AdapterFunc adapter)
{
	return ElementAdapter<Container, AdapterFunc>(std::forward<Container>(container), adapter);
}
#endif