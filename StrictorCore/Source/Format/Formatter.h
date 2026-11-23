#pragma once

#include "Clang/ClangFormat.h"
#include "Clang/CompileDatabase.h"
#include "Clang/SyntaxTree.h"
#include "Clang/Toolchain.h"
#include "Rules/Diagnostic.h"
#include "Rules/SourceText.h"
#include "Style/Cascade.h"

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class Script;

struct FormatResult
{
	std::string text;
	std::vector<Diagnostic> diagnostics;
};

class Formatter
{
public:
	Formatter(Toolchain const& tc, std::filesystem::path const& buildDir);
	~Formatter();

	Formatter(Formatter const&) = delete;
	Formatter& operator=(Formatter const&) = delete;

	bool supports(std::filesystem::path const& file) const;
	FormatResult format(std::filesystem::path const& file,
	                    std::string const& text,
	                    std::optional<std::string> const& base = {});

private:
	std::string reformat(std::filesystem::path const& file,
	                     std::string const& text,
	                     std::optional<std::string> const& base) const;
	void check(std::filesystem::path const& file,
	           SyntaxTree const& tree,
	           Cascade const& cascade,
	           SourceText const& src,
	           std::vector<Edit>* edits,
	           std::vector<Edit>* late,
	           std::vector<Diagnostic>* diagnostics);
	Script& script(std::filesystem::path const& file);
	CompileDatabase* database(std::filesystem::path const& file);
	CompileCommand command(std::filesystem::path const& file);
	Cascade const* styles(std::filesystem::path const& file);

	ClangFormat clangFormat;
	std::filesystem::path buildDir;
	std::map<std::filesystem::path, std::unique_ptr<CompileDatabase>> databases;
	std::map<std::filesystem::path, std::unique_ptr<Cascade>> cascades;
	std::map<std::filesystem::path, std::unique_ptr<Script>> scripts;
};
