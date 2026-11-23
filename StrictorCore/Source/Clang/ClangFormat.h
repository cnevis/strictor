#pragma once

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

class ClangFormat
{
public:
	explicit ClangFormat(std::filesystem::path const& exe);

	std::string format(std::filesystem::path const& file,
	                   std::string const& text,
	                   std::vector<std::pair<int, int>> const& lines = {}) const;

private:
	static std::string rejoinClosers(std::string const& text);

	std::filesystem::path exe;
};
