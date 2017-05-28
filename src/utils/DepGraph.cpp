#include "../common_headers.h"
#include "DepGraph.h"

void DepGraph::work()
{
	std::stack<size_t> S;
	trajan(0, S);
	sort();
}

void DepGraph::trajan(size_t idx, std::stack<size_t> &S)
{
	V[idx].visited = true;
	V[idx].lowlink = V[idx].dfn = dfsclock++;
	S.push(idx);

	for (size_t next : V[idx].to)
	{
		if (!V[next].visited)
		{
			trajan(next, S);
			V[idx].lowlink = std::min(V[idx].lowlink, V[next].lowlink);
		}
		else if (V[next].groupID == -1)
		{
			V[idx].lowlink = std::min(V[idx].lowlink, V[next].dfn);
		}
	}

	if (V[idx].lowlink == V[idx].dfn)
	{
		VGroup.emplace_back();
		while (true)
		{
			size_t v = S.top();
			S.pop();
			VGroup.back().vtxs.push_back(v);
			V[v].groupID = VGroup.size() - 1;

			if (v == idx)
				break;
		}
	}
}

void DepGraph::sort()
{
	for (vertex &v : V)
		for (size_t next : v.to)
			VGroup[V[next].groupID].indegree++;

	std::queue<size_t> worklist;
	for (size_t i = 0; i < VGroup.size(); i++)
		if (VGroup[i].indegree == 0)
			worklist.push(i);

	size_t sorted = 0;
	while (!worklist.empty())
	{
		size_t cur = worklist.front();
		worklist.pop();

		for (size_t vtx : VGroup[cur].vtxs)
			for (size_t next : V[vtx].to)
			{
				VGroup[V[next].groupID].indegree--;
				if (VGroup[V[next].groupID].indegree == 0)
					worklist.push(V[next].groupID);
			}

		std::swap(VGroup[sorted++], VGroup[cur]);
	}
}