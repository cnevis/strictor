#pragma once

#include "Style/Node.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

struct SyntaxNode
{
	Node node;
	std::string name;
	std::string owner;
	bool checkName = false;
	int line = 0;
	int column = 0;
	int firstLine = 0;
	int lastLine = 0;
	std::size_t start = 0;
	std::size_t end = 0;
	std::size_t nameOffset = 0;
	std::size_t bodyStart = 0;
	std::size_t bodyEnd = 0;
	bool bodyBraced = false;
	int clauses = 0;
	std::size_t operatorOffset = 0;
	std::string operatorSpelling;
	SyntaxNode* parent = nullptr;
	std::vector<SyntaxNode*> children;
};

class SyntaxTree
{
public:
	SyntaxNode* add(SyntaxNode* parent, std::string const& type);

	std::vector<std::unique_ptr<SyntaxNode>> const& nodes() const;

private:
	std::vector<std::unique_ptr<SyntaxNode>> all;
};
