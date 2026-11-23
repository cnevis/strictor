#pragma once

#include "Rules/Diagnostic.h"
#include "Rules/Properties.h"

#include <string>
#include <vector>

struct Identifier
{
	std::string label;
	std::string name;
	int line = 0;
	int column = 0;
};

void checkNaming(Identifier const& id, Properties const& props, std::vector<Diagnostic>* result);
