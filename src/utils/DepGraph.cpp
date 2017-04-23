#include "common_headers.h"
#include "DepGraph.h"
#include <exception>

DepGraph::DepGraph(size_t cntVertex) : vtxs(cntVertex) {}

void DepGraph::link(size_t u, size_t v)
{
	if (u >= vtxs.size() || v >= vtxs.size())
		throw std::out_of_range("vertex index out of range");
	edges.emplace_back(edge{ v, vtxs[u].firstEdge });
	vtxs[u].firstEdge = edges.size() - 1;
}