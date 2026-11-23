#include "App/Request.h"

#include <stdexcept>
#include <string>

static char const g_Usage[] =
	"usage: strictor file    [-p <build-dir>] [--whole] <path>...\n"
	"       strictor changed [-p <build-dir>] [--whole]\n"
	"       strictor stdin   [-p <build-dir>] [--whole] <path>";

[[noreturn]] static void reject(std::string const& msg)
{
	throw std::runtime_error(msg + "\n" + g_Usage);
}

Request parseRequest(int argc, wchar_t** argv)
{
	if (argc < 2)
	{
		reject("missing verb");
	}

	Request req;
	std::wstring verb = argv[1];

	if (verb == L"file")
	{
		req.verb = Verb::FILE;
	}
	else if (verb == L"changed")
	{
		req.verb = Verb::CHANGED;
	}
	else if (verb == L"stdin")
	{
		req.verb = Verb::STDIN;
	}
	else
	{
		reject("unknown verb");
	}

	for (int i = 2; i < argc; i++)
	{
		std::wstring arg = argv[i];

		if (arg == L"-p")
		{
			if (i + 1 == argc)
			{
				reject("-p needs a directory");
			}

			i++;
			req.buildDir = std::filesystem::absolute(argv[i]);
			continue;
		}

		if (arg == L"--whole")
		{
			req.whole = true;
			continue;
		}

		if (arg.starts_with(L"-"))
		{
			reject("unknown option");
		}

		req.paths.push_back(arg);
	}

	switch (req.verb)
	{
		case Verb::FILE:
		{
			if (req.paths.empty())
			{
				reject("file needs at least one path");
			}

			break;
		}

		case Verb::CHANGED:
		{
			if (not req.paths.empty())
			{
				reject("changed takes no paths");
			}

			break;
		}

		case Verb::STDIN:
		{
			if (req.paths.size() != 1)
			{
				reject("stdin needs exactly one path");
			}

			break;
		}
	}

	return req;
}
