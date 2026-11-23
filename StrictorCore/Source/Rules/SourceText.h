#pragma once

#include <cstddef>
#include <string>
#include <vector>

struct Edit
{
	std::size_t offset = 0;
	std::size_t length = 0;
	std::string text;
};

class SourceText
{
public:
	explicit SourceText(std::string const& text);

	int lineCount() const;
	std::size_t lineStart(int line) const;
	int line(std::size_t offset) const;
	bool blank(int line) const;
	int blankLinesAbove(int line) const;
	std::string const& eol() const;
	std::string const& content() const;

	std::string apply(std::vector<Edit> edits) const;

private:
	std::string text;
	std::vector<std::size_t> starts;
	std::string newline;
};
