#include "../common_headers.h"
#include "DomTree.h"

void DomTree::buildTree(size_t root)
{
	for (size_t idx = 0; idx < V.size(); idx++)
		V[idx].visited = false, V[idx].df.clear();
	dfs(root);
	for (size_t idx = 0; idx < V.size(); idx++)
		idv[V[idx].dfn] = idx;
	calcIdom();
	for (size_t idx = 0; idx < V.size(); idx++)
		if(V[idx].idom != idx)
			V[V[idx].idom].children.push_back(idx);
	IF_DEBUG(verifyIdom());
	calcDomFrontier();
}

void DomTree::dfs(size_t idx)
{
	V[idx].dfn = idv.size();
	V[idx].visited = true;
	V[idx].semi = size_t(-1);
	V[idx].idom = size_t(-1);
	idv.push_back(idx);
	for (size_t next : V[idx].to)
	{
		if (V[next].visited)
			continue;
		dfs(next);
	}
}

void DomTree::calcIdom()
{
	struct ufsnode	//NOTE: non-standard UFS
	{
		size_t root;
		size_t min_semi_point;
	};
	std::vector<ufsnode> ufs(V.size());
	for (size_t i = 0; i < V.size(); i++)
		ufs[i] = { i, i };

	std::function<size_t(size_t)> ufs_find_root;
	ufs_find_root = [&ufs, &ufs_find_root, this](size_t idx)
	{
		if (ufs[idx].root == idx)
			return idx;
		size_t root = ufs_find_root(ufs[idx].root);
		size_t curSemi = V[ufs[idx].min_semi_point].semi;
		size_t parSemi = V[ufs[ufs[idx].root].min_semi_point].semi;
		if (V[parSemi].dfn < V[curSemi].dfn)
			ufs[idx].min_semi_point = ufs[ufs[idx].root].min_semi_point;
		return ufs[idx].root = root;
	};
	auto ufs_merge = [&ufs, &ufs_find_root](size_t fa, size_t child)
	{
		assert(ufs[child].root == child);
		ufs[child].root = fa;
	};

	std::vector<std::vector<size_t>> semiDomList(V.size());

	for (ssize_t dfn = idv.size() - 1; dfn >= 0; dfn--)
	{
		size_t idx = idv[dfn];
		if (dfn != 0)
		{
			size_t semi_dfn = SIZE_MAX;
			for (size_t prev : V[idx].from)
			{
				if (V[prev].dfn <= V[idx].dfn)
					semi_dfn = std::min(semi_dfn, V[prev].dfn);
				else
				{
					ufs_find_root(prev);
					size_t minSemi = V[ufs[prev].min_semi_point].semi;
					if (V[minSemi].dfn < semi_dfn)
						semi_dfn = V[minSemi].dfn;
				}
			}
			assert(semi_dfn != SIZE_MAX);
			V[idx].semi = idv[semi_dfn];
		}
		else
			V[idx].semi = 0;
		
		semiDomList[V[idx].semi].push_back(idx);
		
		for (size_t dom_child : semiDomList[idx])
		{
			ufs_find_root(dom_child);
			size_t u = ufs[dom_child].min_semi_point;
			if (V[u].semi == idx)
			{
				V[dom_child].idom = idx;
				assert(V[dom_child].semi == V[dom_child].idom);
			}
			else
			{
				V[dom_child].idom = u;
				assert(V[dom_child].semi != V[dom_child].idom);
			}
		}

		for (size_t next : V[idx].to)
		{
			if (V[next].dfn < V[idx].dfn)	//backward edge & cross edge
				continue;
			if (ufs[next].root != next)		//forward edge
				continue;
			ufs_merge(idx, next);			//tree edge
		}
	}

	for (size_t dfn = 0; dfn < V.size(); dfn++)
	{
		size_t idx = idv[dfn];
		if (V[idx].semi != V[idx].idom)
			V[idx].idom = V[V[idx].idom].idom;
	}
}

void DomTree::calcDomFrontier()
{
	std::vector<std::set<size_t>> frontier(V.size());
	for (size_t idx = 0; idx < V.size(); idx++)
	{
		for (size_t prev : V[idx].from)
		{
			size_t cur = prev;
			while (cur != V[idx].idom)
			{
				frontier[cur].insert(idx);
				cur = V[cur].idom;
			}
		}
	}
	for (size_t idx = 0; idx < V.size(); idx++)
		std::copy(frontier[idx].begin(), frontier[idx].end(), std::back_inserter(V[idx].df));
}

void DomTree::verifyIdom()
{
	std::vector<size_t> idomTrue(V.size(), 0);
	for (size_t dfn = 1; dfn < V.size(); dfn++)
	{
		for (size_t idx = 0; idx < V.size(); idx++)
			V[idx].visited = false;
		std::function<void(size_t)> traverse;
		traverse = [dfn, &traverse, this](size_t idx)
		{
			V[idx].visited = true;
			for (size_t next : V[idx].to)
				if (!V[next].visited && V[next].dfn != dfn)
					traverse(next);
		};
		traverse(0);
		for (size_t idx = 0; idx < V.size(); idx++)
			if (!V[idx].visited && idx != idv[dfn])
				idomTrue[idx] = idv[dfn];
	}
	for (size_t idx = 0; idx < V.size(); idx++)
	{
		assert(idomTrue[idx] == V[idx].idom);
	}
}