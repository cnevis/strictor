#pragma once

#include "Rules/Diagnostic.h"
#include "Rules/SourceText.h"

#include <string>
#include <utility>
#include <vector>

class LineDelta
{
public:
	LineDelta(std::string const& base, std::string const& text);

	bool empty() const;
	bool contains(int line) const;
	std::vector<std::pair<int, int>> ranges() const;

	void filter(SourceText const& src, std::vector<Edit>* edits) const;
	void filter(std::vector<Diagnostic>* diagnostics) const;

private:
	std::vector<bool> lines;
};
