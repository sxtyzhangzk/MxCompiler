#ifndef MX_COMPILER_UTILS_DEP_GRAPH_H
#define MX_COMPILER_UTILS_DEP_GRAPH_H

#include "common.h"
#include <vector>

class DepGraph
{
public:
	DepGraph(size_t cntVertex);
	void link(size_t u, size_t v);
	std::vector<size_t> nextVertex();

protected:
	struct vertex
	{
		size_t firstEdge;
		vertex() : firstEdge(-1) {}
	};
	struct edge
	{
		size_t to;
		size_t nextEdge;
	};

protected:
	void trajan(size_t idx);

protected:
	std::vector<vertex> vtxs;
	std::vector<edge> edges;
};

#endif