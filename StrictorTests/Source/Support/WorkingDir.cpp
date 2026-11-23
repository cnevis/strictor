#include "Support/WorkingDir.h"

WorkingDir::WorkingDir(std::filesystem::path const& dir)
	: previous(std::filesystem::current_path())
{
	std::filesystem::current_path(dir);
}

WorkingDir::~WorkingDir()
{
	std::filesystem::current_path(previous);
}
