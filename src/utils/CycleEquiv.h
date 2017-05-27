#ifndef MX_COMPILER_UTILS_CYCLE_EQUIV_H
#define MX_COMPILER_UTILS_CYCLE_EQUIV_H

#include "../common.h"

class CycleEquiv
{
public:
	CycleEquiv(size_t nVertex) : V(nVertex), dfv(nVertex) {}
	size_t addEdge(size_t u, size_t v)
	{
		if (u != v)
		{
			V[u].to.push_back(v);
			V[u].edges.push_back(E.size());
			V[v].to.push_back(u);
			V[v].edges.push_back(E.size());
		}
		E.emplace_back();
		return E.size() - 1;
	}
	std::vector<std::set<size_t>> work();

protected:
	void dfs(size_t idx, size_t parent);
	void dfs2(size_t idx, size_t upperEdge, std::list<size_t> &bracketList);

protected:
	struct edge
	{
		bool backward = false;
		size_t min_dfn;
		std::list<size_t>::iterator iter;

		struct compactName 
		{
			size_t listFront = size_t(-1); size_t listSize = size_t(-1);
			bool operator<(const compactName &rhs) const
			{
				if (listFront == rhs.listFront)
					return listSize < rhs.listSize;
				return listFront < rhs.listFront;
			}
		};
		compactName name;
	};
	struct vertex
	{
		std::vector<size_t> edges;
		std::vector<size_t> to;
		size_t dfn;
		size_t min_dfn;
		bool visited;
	};
	std::vector<vertex> V;
	std::vector<size_t> dfv;	//dfv[i]: vertex with dfn i
	std::vector<edge> E;
	std::list<size_t> bracketList;
	size_t nEdge;
};

#endif