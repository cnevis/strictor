#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct ProcessResult
{
	int exitCode = 0;
	std::string output;
	std::string error;
};

ProcessResult runProcess(std::filesystem::path const& exe,
                         std::vector<std::string> const& args,
                         std::string const& input,
                         std::filesystem::path const& dir);
