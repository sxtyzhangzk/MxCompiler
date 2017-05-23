#ifndef MX_COMPILER_UTILS_MAX_CLIQUE_H
#define MX_COMPILER_UTILS_MAX_CLIQUE_H

#include "../common.h"

class MaxClique
{
public:
	MaxClique(size_t nVertex) : V(nVertex) {}
	void link(size_t u, size_t v)
	{
		V.at(u).neighbor.insert(v);
		V.at(v).neighbor.insert(u);
	}
	const std::vector<size_t> & findMaxClique()
	{
		BronKerbosch();
		return R;
	}
	bool BronKerbosch();

protected:
	struct vertex
	{
		std::set<size_t> neighbor;
	};
	std::vector<size_t> R, P, X;
	std::vector<vertex> V;
};

#endif