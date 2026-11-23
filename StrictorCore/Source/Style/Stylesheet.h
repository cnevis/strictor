#pragma once

#include "Style/Node.h"

#include <filesystem>
#include <string>
#include <vector>

struct Condition
{
	std::string attribute;
	std::string value;
};

struct Compound
{
	std::string type;
	std::vector<Condition> conditions;

	bool matches(Node const& node) const;
	int specificity() const;
};

enum class Combinator
{
	CHILD,
	DESCENDANT
};

struct Selector
{
	std::vector<Compound> parts;
	std::vector<Combinator> combinators;

	bool matches(Node const& node) const;
	int specificity() const;
};

struct Declaration
{
	std::string property;
	std::vector<std::string> values;
};

struct Rule
{
	std::vector<Selector> selectors;
	std::vector<Declaration> declarations;
};

bool validNodeType(std::string const& type);

class Stylesheet
{
public:
	explicit Stylesheet(std::filesystem::path const& file);

	std::vector<Rule> const& rules() const;

private:
	std::vector<Rule> parsed;
};
