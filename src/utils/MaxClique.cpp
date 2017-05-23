#include "../common_headers.h"
#include "MaxClique.h"

bool MaxClique::BronKerbosch()
{
	if (P.empty() && X.empty())
		return true;
	size_t u = P.empty() ? *X.begin() : *P.begin();
	
	size_t j = 0;
	for (size_t i = 0; i < P.size(); i++)
		if (V[u].neighbor.count(P[i]))
			std::swap(P[j++], P[i]);
	for (size_t k = P.size() - 1; k >= j; k--)
	{
		size_t v = P[k];
		std::vector<size_t> oldP = std::move(P), oldX = std::move(X);
		for (size_t t : oldP)
			if (V[v].neighbor.count(t))
				P.push_back(t);
		for (size_t t : oldX)
			if (V[v].neighbor.count(t))
				X.push_back(t);
		R.push_back(v);
		if (BronKerbosch())
			return true;
		R.pop_back();
		P = std::move(oldP), X = std::move(oldX);
		assert(k == P.size() - 1 && P[k] == v);
		P.pop_back();
		X.push_back(v);
	}
	return false;
}