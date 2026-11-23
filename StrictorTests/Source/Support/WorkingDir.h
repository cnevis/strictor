#pragma once

#include <filesystem>

class WorkingDir
{
public:
	explicit WorkingDir(std::filesystem::path const& dir);
	~WorkingDir();

	WorkingDir(WorkingDir const&) = delete;
	WorkingDir& operator=(WorkingDir const&) = delete;

private:
	std::filesystem::path previous;
};
