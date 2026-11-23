#pragma once

#include "Rules/Diagnostic.h"
#include "Rules/SourceText.h"

#include <cstddef>
#include <string>
#include <vector>

struct Spacing
{
	int first = 0;
	int last = 0;
	int before = -1;
	int after = -1;
};

struct Declarator
{
	std::size_t start = 0;
	std::size_t name = 0;
	std::size_t end = 0;
	int line = 0;
	int column = 0;
};

struct CaseBody
{
	std::size_t start = 0;
	std::size_t end = 0;
	bool braced = false;
};

void spaceStatements(SourceText const& src, std::vector<Spacing> const& statements, std::vector<Edit>* result);
void convertOperator(SourceText const& src,
                     std::size_t offset,
                     std::string const& spelling,
                     std::string const& mode,
                     std::vector<Edit>* result);
void spaceNot(SourceText const& src, std::size_t offset, std::string const& spelling, std::vector<Edit>* result);
void splitDeclarators(SourceText const& src,
                      std::vector<Declarator> const& group,
                      std::vector<Edit>* edits,
                      std::vector<Diagnostic>* diagnostics);
void braceCases(SourceText const& src, std::vector<CaseBody> const& bodies, std::vector<Edit>* result);
void checkClauses(int line, int column, int clauses, int max, std::vector<Diagnostic>* result);
