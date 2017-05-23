#ifndef MX_COMPILER_UTILS_DOM_TREE_H
#define MX_COMPILER_UTILS_DOM_TREE_H

#include "../common.h"

class DomTree
{
public:
	DomTree() {}
	DomTree(size_t nVertex) : V(nVertex) {}

	size_t addVertex() 
	{
		V.emplace_back();
		return V.size() - 1;
	}
	DomTree & link(size_t u, size_t v)
	{
		V.at(u).to.push_back(v);
		V.at(v).from.push_back(u);
		return *this;
	}
	size_t getIdom(size_t idx)
	{
		return V.at(idx).idom;
	}
	const std::vector<size_t> & getDomFrontier(size_t idx)
	{
		return V.at(idx).df;
	}
	const std::vector<size_t> & getDomChildren(size_t idx)
	{
		return V.at(idx).children;
	}
	void buildTree(size_t root);
	

protected:
	void dfs(size_t idx);
	void calcIdom();
	void calcDomFrontier();
	void verifyIdom();

protected:
	struct vertex
	{
		size_t dfn;
		size_t semi;
		size_t idom;
		std::vector<size_t> to, from;
		std::vector<size_t> df;			//Dominance Frontier
		std::vector<size_t> children;	//Children on dominator tree
		bool visited;
	};
	std::vector<vertex> V;
	std::vector<size_t> idv;	//dfn -> idx
};

#endif