#include "common_headers.h"
#include "IssueCollector.h"
#include <antlr4-runtime.h>

IssueCollector *IssueCollector::defIC = nullptr;

IssueCollector::IssueCollector(issueLevel printLevel, std::ostream *printTarget, const antlr4::TokenStream *tokenStream, const std::string &fileName) :
	printLevel(printLevel), printTarget(printTarget), tokenStream(tokenStream), fileName(fileName), cntError(0)
{
	std::string fileContent = tokenStream->getTokenSource()->getInputStream()->toString();
	std::string temp;
	for (char c : fileContent)
	{
		if (c == '\r')
			continue;
		else if (c == '\n')
		{
			lineContent.push_back(temp);
			temp = "";
		}
		else
			temp += c;
	}
	if (!temp.empty())
		lineContent.push_back(temp);
}

void IssueCollector::notice(ssize_t tokenL, ssize_t tokenR, const std::string &description)
{
	issue e{ NOTICE, tokenL, tokenR, description };
	vIssues.push_back(e);
	printIssue(e);
}
void IssueCollector::warning(ssize_t tokenL, ssize_t tokenR, const std::string &description)
{
	issue e{ WARNING, tokenL, tokenR, description };
	vIssues.push_back(e);
	printIssue(e);
}
void IssueCollector::error(ssize_t tokenL, ssize_t tokenR, const std::string &description)
{
	issue e{ ERROR, tokenL, tokenR, description };
	vIssues.push_back(e);
	printIssue(e);
	cntError++;
	if (cntError >= MAX_ERROR)
		fatal(0, -1, "Maximum error limit exceeded. Stop compiling.");
}
void IssueCollector::fatal(ssize_t tokenL, ssize_t tokenR, const std::string &description)
{
	issue e{ FATAL, tokenL, tokenR, description };
	vIssues.push_back(e);
	throw FatalErrorException{ e };
}

void IssueCollector::printIssue(const issue &e)
{
	static const size_t maxLine = 10;
	static const std::vector<std::string> issueLevelName = { "Notice", "Warning", "Error", "Fatal Error" };
	if (e.level >= printLevel && printTarget && tokenStream)
	{
		if (!fileName.empty())
			*printTarget << fileName << ":";
		if (e.tokenL > e.tokenR)
			*printTarget << " " << issueLevelName[e.level] << ": " << e.description << std::endl;
		else
		{
			//TODO: Tabstop
			size_t startLine = tokenStream->get(e.tokenL)->getLine();
			size_t startPos = tokenStream->get(e.tokenL)->getCharPositionInLine();
			*printTarget << startLine << ":" << startPos << ": " << issueLevelName[e.level] << ": ";
			*printTarget << e.description << std::endl;

			auto *endToken = tokenStream->get(e.tokenR);
			size_t endLine = endToken->getLine();
			size_t endPos = endToken->getCharPositionInLine() + endToken->getStopIndex() - endToken->getStartIndex();
			
			if (startLine == endLine)
				printLine(lineContent.at(startLine - 1), startPos, endPos);
			else
			{
				printLine(lineContent.at(startLine - 1), startPos, startPos);
				for (size_t i = startLine + 1; i <= endLine && i <= startLine + maxLine; i++)
					printLine(lineContent.at(i - 1), 1, 0);
			}
		}
	}
}

void IssueCollector::printLine(const std::string &line, size_t l, size_t r)
{
	static const size_t tabStop = 4;
	auto getHighlightChar = [l, r](size_t pos)
	{
		if (pos < l)
			return ' ';
		if (pos == l)
			return '^';
		if (pos > l && pos <= r)
			return '~';
		return ' ';
	};
	std::string highlight;
	*printTarget << ' ';
	for (size_t i = 0; i < line.size(); i++)
	{
		if (line[i] == '\t')
		{
			for (size_t j = i; j > i && j % tabStop == 0; j++)
			{
				*printTarget << ' ';
				highlight += getHighlightChar(i);
			}
		}
		else
		{
			*printTarget << line[i];
			highlight += getHighlightChar(i);
		}
	}
	*printTarget << std::endl;
	if (l <= r)
		*printTarget << ' ' << highlight << std::endl;
}