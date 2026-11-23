#pragma once

#include "Rules/Diagnostic.h"
#include "Rules/Properties.h"

#include <filesystem>
#include <string>
#include <vector>

struct FreeFunction
{
	std::string name;
	int line = 0;
	int column = 0;
	std::string owner;
	bool staticInClassFile = false;
	bool oneClass = false;
	std::filesystem::path path;
};

void checkHelper(FreeFunction const& f, Properties const& props, std::vector<Diagnostic>* result);
