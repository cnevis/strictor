#include "Clang/ClangFormat.h"

#include "System/Process.h"
#include "System/Text.h"

#include <format>
#include <stdexcept>
#include <vector>

ClangFormat::ClangFormat(std::filesystem::path const& exe)
	: exe(exe)
{
}

std::string ClangFormat::format(std::filesystem::path const& file,
                                std::string const& text,
                                std::vector<std::pair<int, int>> const& lines) const
{
	std::vector<std::string> args = {"--assume-filename=" + utf8(file), "--fallback-style=none"};

	for (auto const& [first, last] : lines)
	{
		args.push_back(std::format("--lines={}:{}", first, last));
	}

	ProcessResult r = runProcess(exe, args, text, {});

	if (r.exitCode != 0)
	{
		throw std::runtime_error("clang-format: " + trim(r.error));
	}

	return rejoinClosers(r.output);
}

std::string ClangFormat::rejoinClosers(std::string const& text)
{
	std::vector<std::string> lines;
	std::size_t pos = 0;

	while (pos < text.size())
	{
		std::size_t end = text.find('\n', pos);
		end = end == std::string::npos ? text.size() : end + 1;
		lines.push_back(text.substr(pos, end - pos));
		pos = end;
	}

	std::string result;

	for (std::size_t i = 0; i < lines.size(); i++)
	{
		std::string const& a = lines[i];

		if (i + 1 < lines.size() and trim(a) == "}")
		{
			std::string const& b = lines[i + 1];
			std::string comment = trim(b);
			std::size_t brace = a.find('}');

			if (comment.starts_with("// namespace ") and b.substr(0, b.find('/')) == a.substr(0, brace))
			{
				result += a.substr(0, brace + 1) + " " + comment + b.substr(b.find_last_not_of("\r\n") + 1);
				i++;
				continue;
			}
		}

		result += a;
	}

	return result;
}
