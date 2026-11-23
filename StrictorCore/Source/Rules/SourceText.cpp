#include "Rules/SourceText.h"

#include <algorithm>

SourceText::SourceText(std::string const& text)
	: text(text)
	, newline("\n")
{
	starts.push_back(0);

	for (std::size_t i = 0; i < text.size(); i++)
	{
		if (text[i] == '\n')
		{
			starts.push_back(i + 1);
		}
	}

	std::size_t first = text.find('\n');

	if (first != std::string::npos and first > 0 and text[first - 1] == '\r')
	{
		newline = "\r\n";
	}
}

int SourceText::lineCount() const
{
	return (int)starts.size();
}

std::size_t SourceText::lineStart(int line) const
{
	if (line < 1)
	{
		return 0;
	}

	if (line > lineCount())
	{
		return text.size();
	}

	return starts[(std::size_t)line - 1];
}

int SourceText::line(std::size_t offset) const
{
	std::vector<std::size_t>::const_iterator it = std::upper_bound(starts.begin(), starts.end(), offset);

	return (int)(it - starts.begin());
}

bool SourceText::blank(int line) const
{
	if (line < 1 or line > lineCount())
	{
		return false;
	}

	std::size_t end = lineStart(line + 1);

	for (std::size_t i = lineStart(line); i < end; i++)
	{
		char c = text[i];

		if (c != ' ' and c != '\t' and c != '\r' and c != '\n')
		{
			return false;
		}
	}

	return line < lineCount() or lineStart(line) < text.size();
}

int SourceText::blankLinesAbove(int line) const
{
	int n = 0;

	for (int l = line - 1; l >= 1 and blank(l); l--)
	{
		n++;
	}

	return n;
}

std::string const& SourceText::eol() const
{
	return newline;
}

std::string const& SourceText::content() const
{
	return text;
}

std::string SourceText::apply(std::vector<Edit> edits) const
{
	std::stable_sort(edits.begin(), edits.end(), [](Edit const& a, Edit const& b)
	{
		return a.offset > b.offset;
	});

	std::string s = text;
	std::size_t limit = text.size();

	for (Edit const& e : edits)
	{
		if (e.offset + e.length > limit)
		{
			continue;
		}

		s.replace(e.offset, e.length, e.text);
		limit = e.offset;
	}

	return s;
}
