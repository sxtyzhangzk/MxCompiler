#ifndef MX_COMPILER_UTILS_DEP_GRAPH_H
#define MX_COMPILER_UTILS_DEP_GRAPH_H

#include "common.h"
#include <vector>

class DepGraph
{
public:
	DepGraph(size_t cntVertex) : V(cntVertex) {}
	void link(size_t u, size_t v)
	{
		V.at(u).to.push_back(v);
		V.at(v).to.push_back(u);
	}
	void work();
	const std::vector<size_t> & getVertex(size_t i)
	{
		return VGroup.at(i).vtxs;
	}

protected:
	struct vertex
	{
		std::vector<size_t> to;

		bool visited = false;
		size_t dfn, lowlink;
		ssize_t groupID = -1;
	};
	struct vtxGroup
	{
		std::vector<size_t> vtxs;
		size_t indegree = 0;
	};

protected:
	void trajan(size_t idx, std::stack<size_t> &S);
	void sort();

protected:
	std::vector<vertex> V;

	size_t dfsclock;
	std::vector<vtxGroup> VGroup;
};

#endif