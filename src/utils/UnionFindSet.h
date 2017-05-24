#ifndef MX_COMPILER_UTILS_UNION_FIND_SET_H
#define MX_COMPILER_UTILS_UNION_FIND_SET_H

#include "../common.h"

class UnionFindSet
{
public:
	UnionFindSet() {}
	UnionFindSet(size_t size) : vNodes(size)
	{
		for (size_t i = 0; i < size; i++)
			vNodes[i].father = i, vNodes[i].size = 1;
	}

	size_t addNode()
	{
		size_t idx = vNodes.size();
		vNodes.push_back({ idx, 1 });
		return idx;
	}
	size_t merge(size_t u, size_t v)
	{
		u = findRoot(u), v = findRoot(v);
		if (u == v)
			return u;
		if (vNodes.at(u).size < vNodes.at(v).size)
			return merge(v, u);
		
		vNodes[v].father = vNodes[u].father;
		size_t root = findRoot(v);
		vNodes[root].size += vNodes[v].size;
		return root;
	}
	size_t findRoot(size_t idx)
	{
		size_t &father = vNodes.at(idx).father;
		if (vNodes[father].father == father)
			return father;
		return father = findRoot(father);
	}

protected:
	struct node
	{
		size_t father;
		size_t size;
	};
	std::vector<node> vNodes;
};

#endif