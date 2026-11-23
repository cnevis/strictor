#pragma once

#include "Clang/CompileDatabase.h"
#include "Clang/SyntaxTree.h"

#include <clang-c/Index.h>

#include <filesystem>
#include <string>
#include <vector>

class TranslationUnit
{
public:
	TranslationUnit(std::filesystem::path const& file, std::string const& text, CompileCommand const& cmd);
	~TranslationUnit();

	TranslationUnit(TranslationUnit const&) = delete;
	TranslationUnit& operator=(TranslationUnit const&) = delete;

	SyntaxTree tree() const;

private:
	std::filesystem::path source;
	CXIndex index = nullptr;
	CXTranslationUnit unit = nullptr;
};
