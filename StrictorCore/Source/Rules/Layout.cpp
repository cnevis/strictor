#include "Rules/Layout.h"

#include "System/Text.h"

#include <algorithm>
#include <format>

static bool blank(char c)
{
	return c == ' ' or c == '\t';
}

static bool identifierEnd(char c)
{
	return upperLetter(c) or lowerLetter(c) or digit(c) or c == '_' or c == '>';
}

static std::size_t skipBlanks(std::string const& text, std::size_t pos)
{
	while (pos < text.size() and blank(text[pos]))
	{
		pos++;
	}

	return pos;
}

void spaceStatements(SourceText const& src, std::vector<Spacing> const& statements, std::vector<Edit>* result)
{
	for (std::size_t i = 1; i < statements.size(); i++)
	{
		Spacing const& prev = statements[i - 1];
		Spacing const& next = statements[i];
		int want = std::max(prev.after, next.before);

		if (want < 0 or prev.last >= next.first)
		{
			continue;
		}

		int have = 0;

		while (prev.last + 1 + have < next.first and src.blank(prev.last + 1 + have))
		{
			have++;
		}

		if (have == want)
		{
			continue;
		}

		Edit e;
		e.offset = src.lineStart(prev.last + 1);
		e.length = src.lineStart(prev.last + 1 + have) - e.offset;

		for (int j = 0; j < want; j++)
		{
			e.text += src.eol();
		}

		result->push_back(e);
	}
}

void convertOperator(SourceText const& src,
                     std::size_t offset,
                     std::string const& spelling,
                     std::string const& mode,
                     std::vector<Edit>* result)
{
	std::string const& text = src.content();
	Edit e;
	e.offset = offset;
	e.length = spelling.size();

	if (mode == "words")
	{
		if (spelling == "&&")
		{
			e.text = "and";
		}
		else if (spelling == "||")
		{
			e.text = "or";
		}
		else if (spelling == "!")
		{
			bool spaced = offset + 1 < text.size() and (text[offset + 1] == ' ' or text[offset + 1] == '\t');
			e.text = spaced ? "not" : "not ";
		}
		else
		{
			return;
		}

		result->push_back(e);
		return;
	}

	if (spelling == "and")
	{
		e.text = "&&";
	}
	else if (spelling == "or")
	{
		e.text = "||";
	}
	else if (spelling == "not")
	{
		e.text = "!";

		while (offset + e.length < text.size() and (text[offset + e.length] == ' ' or text[offset + e.length] == '\t'))
		{
			e.length++;
		}
	}
	else
	{
		return;
	}

	result->push_back(e);
}

void spaceNot(SourceText const& src, std::size_t offset, std::string const& spelling, std::vector<Edit>* result)
{
	std::string const& text = src.content();
	std::size_t next = offset + spelling.size();

	if (spelling != "not" or next >= text.size())
	{
		return;
	}

	if (text[next] == ' ' or text[next] == '\t' or text[next] == '\r' or text[next] == '\n')
	{
		return;
	}

	result->push_back({next, 0, " "});
}

void splitDeclarators(SourceText const& src,
                      std::vector<Declarator> const& group,
                      std::vector<Edit>* edits,
                      std::vector<Diagnostic>* diagnostics)
{
	std::string const& text = src.content();
	Declarator const& first = group.front();
	std::size_t cut = first.name;

	while (cut > first.start and (blank(text[cut - 1]) or text[cut - 1] == '*' or text[cut - 1] == '&'))
	{
		cut--;
	}

	std::string prefix = text.substr(first.start, cut - first.start);
	std::size_t semicolon = skipBlanks(text, group.back().end);
	bool clean = not prefix.empty() and identifierEnd(prefix.back());

	if (not clean or semicolon >= text.size() or text[semicolon] != ';')
	{
		diagnostics->push_back({first.line, first.column, "layout.declarators", "declare one variable per declaration"});
		return;
	}

	std::size_t lineStart = src.lineStart(src.line(first.start));
	std::string indent = text.substr(lineStart, skipBlanks(text, lineStart) - lineStart);
	std::string result = text.substr(first.start, first.end - first.start);

	for (std::size_t i = 1; i < group.size(); i++)
	{
		std::size_t comma = skipBlanks(text, group[i - 1].end);

		if (comma >= text.size() or text[comma] != ',')
		{
			return;
		}

		std::string part = trim(text.substr(comma + 1, group[i].end - comma - 1));
		result += ";" + src.eol() + indent + prefix + " " + part;
	}

	edits->push_back({first.start, group.back().end - first.start, result});
}

void braceCases(SourceText const& src, std::vector<CaseBody> const& bodies, std::vector<Edit>* result)
{
	std::string const& text = src.content();

	for (CaseBody const& b : bodies)
	{
		if (b.braced or b.end <= b.start)
		{
			continue;
		}

		std::size_t end = skipBlanks(text, b.end);
		end = end < text.size() and text[end] == ';' ? end + 1 : b.end;

		std::string body = text.substr(b.start, end - b.start);
		result->push_back({b.start, end - b.start, "{" + src.eol() + body + src.eol() + "}"});
	}
}

void checkClauses(int line, int column, int clauses, int max, std::vector<Diagnostic>* result)
{
	if (clauses <= max)
	{
		return;
	}

	std::string msg = std::format("condition has {} clauses; split into guard clauses (max {})", clauses, max);
	result->push_back({line, column, "layout.condition", msg});
}
