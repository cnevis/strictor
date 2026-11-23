#include "App/Request.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <string>
#include <vector>

static Request parse(std::vector<std::wstring> args)
{
	args.insert(args.begin(), L"strictor");

	std::vector<wchar_t*> argv;

	for (std::wstring& a : args)
	{
		argv.push_back(a.data());
	}

	return parseRequest((int)argv.size(), argv.data());
}

TEST_CASE("parseRequest reads each verb with its paths and build directory")
{
	Request file = parse({L"file", L"a.cpp", L"-p", L"build", L"b.h"});

	CHECK(file.verb == Verb::FILE);
	CHECK(file.paths == std::vector<std::filesystem::path>{"a.cpp", "b.h"});
	CHECK(file.buildDir == std::filesystem::absolute("build"));
	CHECK(not file.whole);
	CHECK(parse({L"file", L"--whole", L"a.cpp"}).whole);

	Request changed = parse({L"changed"});

	CHECK(changed.verb == Verb::CHANGED);
	CHECK(changed.paths.empty());
	CHECK(changed.buildDir.empty());

	Request stream = parse({L"stdin", L"-p", L"C:\\b", L"src\\Main.cpp"});

	CHECK(stream.verb == Verb::STDIN);
	CHECK(stream.paths == std::vector<std::filesystem::path>{"src\\Main.cpp"});
	CHECK(stream.buildDir == std::filesystem::path("C:\\b"));

	CHECK(parse({L"file", L"Käyttäjä.cpp"}).paths[0] == std::filesystem::path(L"Käyttäjä.cpp"));
	CHECK(parse({L"changed", L"-p", L"x", L"-p", L"y"}).buildDir == std::filesystem::absolute("y"));
}

TEST_CASE("parseRequest rejects malformed command lines with a usage message")
{
	struct Case
	{
		std::vector<std::wstring> args;
		char const* message;
	};

	Case const cases[] =
	{
		{{}, "missing verb"},
		{{L"format", L"a.cpp"}, "unknown verb"},
		{{L"--help"}, "unknown verb"},
		{{L"file"}, "file needs at least one path"},
		{{L"file", L"-p", L"build"}, "file needs at least one path"},
		{{L"changed", L"a.cpp"}, "changed takes no paths"},
		{{L"stdin"}, "stdin needs exactly one path"},
		{{L"stdin", L"a.cpp", L"b.cpp"}, "stdin needs exactly one path"},
		{{L"file", L"a.cpp", L"-p"}, "-p needs a directory"},
		{{L"file", L"-i", L"a.cpp"}, "unknown option"},
		{{L"changed", L"--all"}, "unknown option"}
	};

	for (Case const& c : cases)
	{
		INFO(c.message);
		CHECK_THROWS_WITH(parse(c.args),
		                  Catch::Matchers::StartsWith(std::string(c.message) + "\nusage: strictor file    [-p <build-dir>] [--whole] <path>..."));
	}
}
