#pragma once

#include <string>

struct Diagnostic
{
	int line = 0;
	int column = 0;
	std::string rule;
	std::string message;
};
