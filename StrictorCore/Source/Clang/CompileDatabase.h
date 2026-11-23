#pragma once

#include <clang-c/CXCompilationDatabase.h>

#include <filesystem>
#include <string>
#include <vector>

struct CompileCommand
{
	std::filesystem::path dir;
	std::vector<std::string> args;
};

class CompileDatabase
{
public:
	explicit CompileDatabase(std::filesystem::path const& dir);
	~CompileDatabase();

	CompileDatabase(CompileDatabase const&) = delete;
	CompileDatabase& operator=(CompileDatabase const&) = delete;

	CompileCommand command(std::filesystem::path const& file) const;

private:
	CXCompilationDatabase db = nullptr;
};
