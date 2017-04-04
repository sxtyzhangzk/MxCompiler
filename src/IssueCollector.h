#ifndef MX_COMPILER_ERROR_COLLECTOR_H
#define MX_COMPILER_ERROR_COLLECTOR_H

#include "common.h"
#include <antlr4-runtime.h>
#include <iostream>
#include <vector>
#include <string>

class IssueCollector
{
public:
	enum issueLevel : int
	{
		NOTICE = 0, WARNING = 1, ERROR = 2, FATAL = 3
	};
	struct issue
	{
		issueLevel level;
		ssize_t tokenL, tokenR;
		std::string description;
	};
	struct FatalErrorException { issue e; };

	std::vector<issue> vIssues;
	size_t cntError;
	
public:
	IssueCollector() : printLevel(FATAL), printTarget(nullptr), tokenStream(nullptr), cntError(0) {}
	IssueCollector(issueLevel printLevel, std::ostream *printTarget, const antlr4::TokenStream *tokenStream, const std::string &fileName);

	void notice(ssize_t tokenL, ssize_t tokenR, const std::string &description);
	void warning(ssize_t tokenL, ssize_t tokenR, const std::string &description);
	void error(ssize_t tokenL, ssize_t tokenR, const std::string &description);
	void fatal(ssize_t tokenL, ssize_t tokenR, const std::string &description);

protected:
	void printIssue(const issue &e);

protected:
	issueLevel printLevel;
	std::ostream *printTarget;
	const antlr4::TokenStream *tokenStream;
	std::string fileName;
	std::vector<std::string> lineContent;
};

#endif