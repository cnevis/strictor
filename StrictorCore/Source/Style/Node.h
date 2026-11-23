#pragma once

#include <map>
#include <string>

struct Node
{
	std::string type;
	std::map<std::string, std::string> attributes;
	Node const* parent = nullptr;
	std::string name;
};
