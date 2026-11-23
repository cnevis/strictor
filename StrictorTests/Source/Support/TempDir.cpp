#include "Support/TempDir.h"

#include "System/Text.h"

#include <windows.h>

#include <atomic>
#include <format>
#include <fstream>
#include <sstream>

static std::atomic<int> g_Counter = 0;

TempDir::TempDir()
{
	std::string name = std::format("strictor-{}-{}-{}", GetCurrentProcessId(), GetTickCount64(), g_Counter++);
	dir = std::filesystem::temp_directory_path() / name;
	std::filesystem::create_directories(dir);
	dir = std::filesystem::canonical(dir);
}

TempDir::~TempDir()
{
	std::error_code ec;
	std::filesystem::remove_all(dir, ec);
}

std::filesystem::path const& TempDir::path() const
{
	return dir;
}

std::filesystem::path TempDir::write(std::string const& name, std::string const& text) const
{
	std::filesystem::path p = dir / utf8Path(name);
	std::filesystem::create_directories(p.parent_path());

	std::ofstream file(p, std::ios::binary | std::ios::trunc);
	file.write(text.data(), (std::streamsize)text.size());

	return p;
}

std::string TempDir::read(std::string const& name) const
{
	std::ifstream in(dir / utf8Path(name), std::ios::binary);
	std::ostringstream ss;
	ss << in.rdbuf();

	return ss.str();
}
