#pragma once

#include <filesystem>

class Toolchain
{
public:
	Toolchain();

	std::filesystem::path clangFormat() const;

private:
	std::filesystem::path bin;
};
