#pragma once

#include <filesystem>
#include <string>

class TempDir
{
public:
	TempDir();
	~TempDir();

	TempDir(TempDir const&) = delete;
	TempDir& operator=(TempDir const&) = delete;

	std::filesystem::path const& path() const;
	std::filesystem::path write(std::string const& name, std::string const& text) const;
	std::string read(std::string const& name) const;

private:
	std::filesystem::path dir;
};
