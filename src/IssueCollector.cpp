#include "IssueCollector.h"

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
			*printTarget << lineContent.at(startLine - 1) << std::endl;
			for (size_t i = 0; i < startPos; i++)
				*printTarget << " ";
			*printTarget << "^";
			if (endLine == startLine)
			{
				for (size_t i = startPos + 1; i <= endPos; i++)
					*printTarget << "~";
				*printTarget << std::endl;
			}
			else
			{
				*printTarget << std::endl;
				for (size_t i = startLine + 1; i <= endLine; i++)
					*printTarget << lineContent.at(i - 1) << std::endl;
			}
		}
	}
}
