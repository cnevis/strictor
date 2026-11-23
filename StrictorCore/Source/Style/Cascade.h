#pragma once

#include "Rules/Properties.h"
#include "Style/Node.h"
#include "Style/Stylesheet.h"

#include <vector>

class Cascade
{
public:
	void append(Stylesheet const& sheet);

	Properties compute(Node const& node) const;

private:
	std::vector<Rule> rules;
};
