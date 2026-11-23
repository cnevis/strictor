#include "Support/Fixtures.h"
#include "Support/TempDir.h"
#include "System/Process.h"
#include "System/Text.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

TEST_CASE("runProcess feeds input and captures output, error, exit code and directory")
{
	ProcessResult echo = runProcess("findstr", {"^"}, "alpha\r\nbeta\r\n", {});

	CHECK(echo.exitCode == 0);
	CHECK(echo.output == "alpha\r\nbeta\r\n");
	CHECK(echo.error.empty());

	ProcessResult failing = runProcess("cmd", {"/c", "echo oops 1>&2 & exit 3"}, "", {});

	CHECK(failing.exitCode == 3);
	CHECK(trim(failing.error) == "oops");
	CHECK(failing.output.empty());

	TempDir tmp;
	ProcessResult cd = runProcess("cmd", {"/c", "cd"}, "", tmp.path());

	CHECK(trim(cd.output) == utf8(tmp.path()));
}

TEST_CASE("runProcess handles input and output larger than the pipe buffers")
{
	std::string big;

	for (int i = 0; i < 100000; i++)
	{
		big += "line " + std::to_string(i) + "\r\n";
	}

	ProcessResult r = runProcess("findstr", {"^"}, big, {});

	CHECK(r.exitCode == 0);
	CHECK(r.output.size() == big.size());
	CHECK(r.output == big);
}

TEST_CASE("runProcess fails for a missing program")
{
	CHECK_THROWS(runProcess("strictor-no-such-program", {}, "", {}));
}

TEST_CASE("runProcess quotes arguments so they arrive unchanged")
{
	std::vector<std::string> paths =
	{
		"plain",
		"with space",
		"quote\"inside",
		"trail\\",
		"space and trail\\",
		"slashes\\\\\"mix",
		"tab\there",
		"Käyttäjä"
	};

	std::vector<std::string> args = {"file"};

	for (std::string const& p : paths)
	{
		args.push_back(p);
	}

	ProcessResult r = runProcess(strictorExe(), args, "", {});

	CHECK(r.exitCode == 2);

	for (std::string const& p : paths)
	{
		INFO(p);
		CHECK(r.error.find(p + ": error: unsupported file type") != std::string::npos);
	}
}
