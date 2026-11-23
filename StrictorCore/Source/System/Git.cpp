#include "System/Git.h"

#include "System/Process.h"
#include "System/Text.h"

#include <exception>
#include <stdexcept>

std::vector<std::filesystem::path> gitChangedFiles()
{
	ProcessResult top = runProcess("git", {"rev-parse", "--show-toplevel"}, "", {});

	if (top.exitCode != 0)
	{
		throw std::runtime_error("not inside a git repository");
	}

	std::filesystem::path dir = utf8Path(trim(top.output));
	ProcessResult status = runProcess("git", {"status", "--porcelain=v1", "-z", "--untracked-files=all"}, "", dir);

	if (status.exitCode != 0)
	{
		throw std::runtime_error("git status failed: " + trim(status.error));
	}

	std::string const& text = status.output;
	std::vector<std::filesystem::path> files;
	std::size_t i = 0;

	while (i < text.size())
	{
		std::size_t end = text.find('\0', i);

		if (end == std::string::npos)
		{
			end = text.size();
		}

		std::string entry = text.substr(i, end - i);
		i = end + 1;

		if (entry[0] == 'R' or entry[0] == 'C')
		{
			std::size_t next = text.find('\0', i);
			i = next == std::string::npos ? text.size() : next + 1;
		}

		if (entry.size() < 4)
		{
			continue;
		}

		std::filesystem::path p = dir / utf8Path(entry.substr(3));

		if (std::filesystem::is_regular_file(p))
		{
			files.push_back(std::filesystem::proximate(p));
		}
	}

	return files;
}

std::optional<std::string> gitHeadText(std::filesystem::path const& file)
{
	std::filesystem::path abs = std::filesystem::absolute(file);
	ProcessResult r;

	try
	{
		r = runProcess("git", {"show", "HEAD:./" + utf8(abs.filename())}, "", abs.parent_path());
	}
	catch (std::exception const&)
	{
		return std::nullopt;
	}

	if (r.exitCode != 0)
	{
		return std::nullopt;
	}

	return r.output;
}
