#include "../common_headers.h"
#include "CycleEquiv.h"

void CycleEquiv::dfs(size_t idx, size_t parent)
{
	V[idx].visited = true;
	V[idx].min_dfn = V[idx].dfn = dfv.size();
	dfv.push_back(idx);
	for (size_t i = 0; i < V[idx].to.size(); i++)
	{
		size_t child = V[idx].to[i];
		if (child == parent)
			continue;
		if (V[child].visited)
		{
			E[V[idx].edges[i]].backward = true;
			E[V[idx].edges[i]].min_dfn = std::min(V[idx].dfn, V[child].dfn);
			V[idx].min_dfn = std::min(V[idx].min_dfn, V[child].dfn);
			continue;
		}
		dfs(child, idx);
		if (V[child].min_dfn < V[idx].min_dfn)
			V[idx].min_dfn = V[child].min_dfn;
	}
}

void CycleEquiv::dfs2(size_t idx, size_t upperEdge, std::list<size_t> &bracketList)
{
	V[idx].visited = true;
	size_t nChild = 0;
	for (size_t i = 0; i < V[idx].to.size(); i++)
	{
		size_t child = V[idx].to[i];
		if (V[child].visited)
			continue;
		nChild++;
		std::list<size_t> tmp;
		dfs2(child, V[idx].edges[i], tmp);
		bracketList.splice(bracketList.end(), tmp);
	}
	for (size_t e : V[idx].edges)
	{
		if (!E[e].backward)
			continue;
		if (E[e].min_dfn == V[idx].dfn)
			bracketList.erase(E[e].iter);
		else
		{
			assert(E[e].min_dfn < V[idx].dfn);
			bracketList.push_front(e);
			E[e].iter = bracketList.begin();
		}
	}
	if (nChild > 1)
	{
		size_t min_dfn = SIZE_MAX, min_dfn2 = SIZE_MAX;
		for (size_t i = 0; i < V[idx].to.size(); i++)
		{
			size_t child = V[idx].to[i];
			if (E[V[idx].edges[i]].backward || V[child].dfn < V[idx].dfn)
				continue;
			if (V[child].min_dfn < min_dfn)
				min_dfn2 = min_dfn, min_dfn = V[child].min_dfn;
			else if (V[child].min_dfn < min_dfn2)
				min_dfn2 = V[child].min_dfn;
		}
		if (min_dfn2 < V[idx].dfn)
		{
			size_t e = E.size();
			E.push_back(edge{ true, min_dfn2 });
			V[dfv[min_dfn2]].edges.push_back(e);
			bracketList.push_front(e);
			E[e].iter = bracketList.begin();
		}
	}
	E[upperEdge].name = { bracketList.front(), bracketList.size() };
}

std::vector<std::set<size_t>> CycleEquiv::work()
{
	nEdge = E.size();
	dfv.clear();
	for (vertex &vtx : V)
		vtx.visited = false;
	dfs(0, SIZE_MAX);
	for (vertex &vtx : V)
		vtx.visited = false;
	V[0].visited = true;
	
	for (size_t i = 0; i < V[0].to.size(); i++)
	{
		if (V[V[0].to[i]].visited)
			continue;
		std::list<size_t> bracketList;
		dfs2(V[0].to[i], V[0].edges[i], bracketList);
	}

	std::map<edge::compactName, std::set<size_t>> equClass;
	for (size_t i = 0; i < nEdge; i++)
	{
		if (E[i].backward)
			equClass[edge::compactName{ i, 1 }].insert(i);
		else
			equClass[E[i].name].insert(i);
	}

	std::vector<std::set<size_t>> ret;
	for (auto &kv : equClass)
		ret.emplace_back(std::move(kv.second));
	return ret;
}