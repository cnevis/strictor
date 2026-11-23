#include "App/Runner.h"

#include "System/Git.h"
#include "System/Text.h"

#include <fcntl.h>
#include <io.h>

#include <algorithm>
#include <exception>
#include <fstream>
#include <print>
#include <sstream>
#include <stdexcept>

static std::string readFile(std::filesystem::path const& p)
{
	std::ifstream in(p, std::ios::binary);

	if (not in)
	{
		throw std::runtime_error("cannot read the file");
	}

	std::ostringstream ss;
	ss << in.rdbuf();

	return ss.str();
}

static void writeFile(std::filesystem::path const& p, std::string const& text)
{
	std::ofstream out(p, std::ios::binary | std::ios::trunc);
	out.write(text.data(), (std::streamsize)text.size());

	if (not out)
	{
		throw std::runtime_error("cannot write the file");
	}
}

static std::string readStdin()
{
	std::string text;
	char buf[4096];
	std::size_t n = 0;

	_setmode(_fileno(stdin), _O_BINARY);

	while ((n = std::fread(buf, 1, sizeof(buf), stdin)) > 0)
	{
		text.append(buf, n);
	}

	return text;
}

Runner::Runner(Toolchain const& tc, std::filesystem::path const& buildDir, bool whole)
	: formatter(tc, buildDir)
	, whole(whole)
{
}

void Runner::files(std::vector<std::filesystem::path> const& paths)
{
	for (std::filesystem::path const& p : paths)
	{
		if (not formatter.supports(p))
		{
			fail(p, "unsupported file type");
			continue;
		}

		try
		{
			std::string text = readFile(p);
			FormatResult r = formatter.format(p, text, base(p));

			if (r.text != text)
			{
				writeFile(p, r.text);
				std::println("fixed  {}", utf8(p));
			}

			report(p, r.diagnostics, stdout);
		}
		catch (std::exception const& e)
		{
			fail(p, e.what());
		}
	}
}

void Runner::changed()
{
	std::vector<std::filesystem::path> sources;

	for (std::filesystem::path const& p : gitChangedFiles())
	{
		if (formatter.supports(p))
		{
			sources.push_back(p);
		}
	}

	files(sources);
}

void Runner::stream(std::filesystem::path const& file)
{
	try
	{
		std::string text = readStdin();
		FormatResult r = formatter.format(file, text, base(file));

		_setmode(_fileno(stdout), _O_BINARY);
		std::fwrite(r.text.data(), 1, r.text.size(), stdout);
		std::fflush(stdout);
		report(file, r.diagnostics, stderr);
	}
	catch (std::exception const& e)
	{
		fail(file, e.what());
	}
}

int Runner::exitCode() const
{
	return code;
}

void Runner::report(std::filesystem::path const& file, std::vector<Diagnostic> const& ds, std::FILE* out)
{
	for (Diagnostic const& d : ds)
	{
		std::println(out, "{}:{}:{}: {}: {}", utf8(file), d.line, d.column, d.rule, d.message);
	}

	if (not ds.empty())
	{
		code = std::max(code, 1);
	}
}

void Runner::fail(std::filesystem::path const& file, std::string const& msg)
{
	std::println(stderr, "{}: error: {}", utf8(file), msg);
	code = 2;
}

std::optional<std::string> Runner::base(std::filesystem::path const& file) const
{
	if (whole)
	{
		return std::nullopt;
	}

	return gitHeadText(file);
}
