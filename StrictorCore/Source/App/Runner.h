#pragma once

#include "Format/Formatter.h"

#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

class Runner
{
public:
	Runner(Toolchain const& tc, std::filesystem::path const& buildDir, bool whole);

	void files(std::vector<std::filesystem::path> const& paths);
	void changed();
	void stream(std::filesystem::path const& file);
	int exitCode() const;

private:
	void report(std::filesystem::path const& file, std::vector<Diagnostic> const& ds, std::FILE* fp);
	void fail(std::filesystem::path const& file, std::string const& msg);
	std::optional<std::string> base(std::filesystem::path const& file) const;

	Formatter formatter;
	bool whole = false;
	int code = 0;
};
