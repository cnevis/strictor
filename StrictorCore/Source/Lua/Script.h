#pragma once

#include "Clang/SyntaxTree.h"
#include "Rules/Diagnostic.h"
#include "Rules/SourceText.h"

#include <sol/sol.hpp>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

class RuleContext
{
public:
	RuleContext(SourceText const& src, std::vector<Diagnostic>* diagnostics, std::vector<Edit>* edits);

	void report(std::string const& rule, SyntaxNode const& node, std::string const& msg);
	void insertBefore(SyntaxNode const& node, std::string const& text);
	SourceText const& source() const;

private:
	SourceText const& src;
	std::vector<Diagnostic>* diagnostics;
	std::vector<Edit>* edits;
};

class Script
{
public:
	explicit Script(std::filesystem::path const& file);

	Script(Script const&) = delete;
	Script& operator=(Script const&) = delete;

	void run(SyntaxNode const& node, RuleContext* ctx);

private:
	sol::state lua;
	std::map<std::string, std::vector<sol::protected_function>> handlers;
};
