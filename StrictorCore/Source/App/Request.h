#pragma once

#include <filesystem>
#include <vector>

enum class Verb
{
	FILE,
	CHANGED,
	STDIN
};

struct Request
{
	Verb verb = Verb::FILE;
	std::filesystem::path buildDir;
	std::vector<std::filesystem::path> paths;
	bool whole = false;
};

Request parseRequest(int argc, wchar_t** argv);
