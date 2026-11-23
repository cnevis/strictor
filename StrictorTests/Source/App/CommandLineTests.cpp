#include "Support/Fixtures.h"
#include "Support/TempDir.h"
#include "System/Process.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <string>
#include <vector>

using Catch::Matchers::ContainsSubstring;

static std::string lf(std::string const& s)
{
	std::string r;

	for (std::size_t i = 0; i < s.size(); i++)
	{
		if (s[i] == '\r' and i + 1 < s.size() and s[i + 1] == '\n')
		{
			continue;
		}

		r.push_back(s[i]);
	}

	return r;
}

static ProcessResult run(TempDir const& tmp, std::vector<std::string> const& args, std::string const& input = "")
{
	ProcessResult r = runProcess(strictorExe(), args, input, tmp.path());
	r.error = lf(r.error);

	if (args.empty() or args[0] != "stdin")
	{
		r.output = lf(r.output);
	}

	return r;
}

static void project(TempDir const& tmp)
{
	tmp.write(".clang-format", "BasedOnStyle: LLVM\n");
	tmp.write(".strictor", "variable[storage=local] { name: camelCase; }\n");
}

static void git(TempDir const& tmp, std::vector<std::string> const& args)
{
	ProcessResult r = runProcess("git", args, "", tmp.path());
	INFO(r.error);
	REQUIRE(r.exitCode == 0);
}

TEST_CASE("strictor file fixes formatting in place and reports names it cannot fix")
{
	TempDir tmp;
	project(tmp);
	tmp.write("a.cpp", "int f( ){int Bad_Name=0;return Bad_Name;}\n");
	tmp.write("clean.cpp", "int g() { return 0; }\n");

	ProcessResult first = run(tmp, {"file", "a.cpp", "clean.cpp"});

	CHECK(first.exitCode == 1);
	CHECK(first.output == "fixed  a.cpp\na.cpp:2:7: naming.case: local 'Bad_Name' should be camelCase\n");
	CHECK(first.error.empty());
	CHECK(tmp.read("a.cpp") == "int f() {\n  int Bad_Name = 0;\n  return Bad_Name;\n}\n");
	CHECK(tmp.read("clean.cpp") == "int g() { return 0; }\n");

	ProcessResult second = run(tmp, {"file", "a.cpp"});

	CHECK(second.exitCode == 1);
	CHECK(second.output == "a.cpp:2:7: naming.case: local 'Bad_Name' should be camelCase\n");
}

TEST_CASE("strictor file exits 0 when everything is clean")
{
	TempDir tmp;
	project(tmp);
	tmp.write("src/clean.cpp", "int g() { return 0; }\n");

	ProcessResult r = run(tmp, {"file", "src/clean.cpp"});

	CHECK(r.exitCode == 0);
	CHECK(r.output.empty());
	CHECK(r.error.empty());
}

TEST_CASE("strictor file exits 2 on failures but still processes the other files")
{
	TempDir tmp;
	project(tmp);
	tmp.write("broken.cpp", "int f() { return nope; }\n");
	tmp.write("notes.txt", "hello\n");
	tmp.write("good.cpp", "int  g( ){return 0;}\n");

	ProcessResult r = run(tmp, {"file", "broken.cpp", "notes.txt", "missing.cpp", "good.cpp"});

	CHECK(r.exitCode == 2);
	CHECK(r.output == "fixed  good.cpp\n");
	CHECK_THAT(r.error, ContainsSubstring("broken.cpp: error: parse failed at 1:18: use of undeclared identifier 'nope'"));
	CHECK_THAT(r.error, ContainsSubstring("notes.txt: error: unsupported file type"));
	CHECK_THAT(r.error, ContainsSubstring("missing.cpp: error: cannot read the file"));
	CHECK(tmp.read("broken.cpp") == "int f() { return nope; }\n");

	tmp.write("bad/.strictor", "function[access=secret] {}\n");
	tmp.write("bad/a.cpp", "int a;\n");

	ProcessResult config = run(tmp, {"file", "bad/a.cpp"});

	CHECK(config.exitCode == 2);
	CHECK_THAT(config.error, ContainsSubstring(".strictor:1: unknown value 'secret' for attribute 'access'"));
}

TEST_CASE("strictor stdin writes formatted code to stdout and problems to stderr")
{
	TempDir tmp;
	project(tmp);

	ProcessResult r = run(tmp, {"stdin", "src/never.cpp"}, "int f( ){int Bad_Name=0;return Bad_Name;}\r\n");

	CHECK(r.exitCode == 1);
	CHECK(r.output == "int f() {\r\n  int Bad_Name = 0;\r\n  return Bad_Name;\r\n}\r\n");
	CHECK(r.error == "src/never.cpp:2:7: naming.case: local 'Bad_Name' should be camelCase\n");
	CHECK_FALSE(std::filesystem::exists(tmp.path() / "src" / "never.cpp"));

	ProcessResult broken = run(tmp, {"stdin", "a.cpp"}, "int x = nope;\n");

	CHECK(broken.exitCode == 2);
	CHECK(broken.output.empty());
	CHECK_THAT(broken.error, ContainsSubstring("a.cpp: error: parse failed"));
}

TEST_CASE("strictor changed processes only the C and C++ files git reports as changed")
{
	TempDir tmp;
	project(tmp);
	tmp.write("same.cpp", "int  same( ){return 0;}\n");
	tmp.write("edit.cpp", "int e() { return 0; }\n");
	git(tmp, {"init", "-q"});
	git(tmp, {"add", "-A"});
	git(tmp, {"-c", "user.name=t", "-c", "user.email=t@t", "commit", "-qm", "init"});

	tmp.write("edit.cpp", "int  e( ){int Bad_Name=0;return Bad_Name;}\n");
	tmp.write("src/new.h", "int  n( );\n");
	tmp.write("notes.txt", "int  x;\n");

	ProcessResult r = run(tmp, {"changed"});

	CHECK(r.exitCode == 1);
	CHECK_THAT(r.output, ContainsSubstring("fixed  edit.cpp\n"));
	CHECK_THAT(r.output, ContainsSubstring("fixed  src\\new.h\n"));
	CHECK_THAT(r.output, ContainsSubstring("edit.cpp:2:7: naming.case: local 'Bad_Name' should be camelCase\n"));
	CHECK(tmp.read("same.cpp") == "int  same( ){return 0;}\n");
	CHECK(tmp.read("notes.txt") == "int  x;\n");
	CHECK(tmp.read("src/new.h") == "int n();\n");

	ProcessResult again = run(tmp, {"changed"});

	CHECK(again.exitCode == 1);
	CHECK(again.output == "edit.cpp:2:7: naming.case: local 'Bad_Name' should be camelCase\n");
}

TEST_CASE("strictor file fixes and reports only lines changed since HEAD unless --whole")
{
	TempDir tmp;
	project(tmp);
	tmp.write("x.cpp", "int  a( ){int Old_Name=0;return Old_Name;}\n\nint b() { return 0; }\n");
	tmp.write("same.cpp", "int  same( ){int Same_Name=0;return Same_Name;}\n");
	git(tmp, {"init", "-q"});
	git(tmp, {"add", "-A"});
	git(tmp, {"-c", "user.name=t", "-c", "user.email=t@t", "commit", "-qm", "init"});

	tmp.write("x.cpp", "int  a( ){int Old_Name=0;return Old_Name;}\n\nint  b( ){int New_Name=1;return New_Name;}\n");

	ProcessResult r = run(tmp, {"file", "x.cpp", "same.cpp"});

	CHECK(r.exitCode == 1);
	CHECK(r.output == "fixed  x.cpp\nx.cpp:4:7: naming.case: local 'New_Name' should be camelCase\n");
	CHECK(tmp.read("x.cpp") == "int  a( ){int Old_Name=0;return Old_Name;}\n\nint b() {\n  int New_Name = 1;\n  return New_Name;\n}\n");
	CHECK(tmp.read("same.cpp") == "int  same( ){int Same_Name=0;return Same_Name;}\n");

	ProcessResult again = run(tmp, {"file", "x.cpp"});

	CHECK(again.exitCode == 1);
	CHECK(again.output == "x.cpp:4:7: naming.case: local 'New_Name' should be camelCase\n");

	ProcessResult whole = run(tmp, {"file", "--whole", "x.cpp"});

	CHECK(whole.exitCode == 1);
	CHECK_THAT(whole.output, ContainsSubstring("fixed  x.cpp\n"));
	CHECK_THAT(whole.output, ContainsSubstring("x.cpp:2:7: naming.case: local 'Old_Name' should be camelCase\n"));
	CHECK(tmp.read("x.cpp").starts_with("int a() {\n  int Old_Name = 0;\n"));
}

TEST_CASE("strictor changed fails outside a git repository")
{
	TempDir tmp;
	ProcessResult r = run(tmp, {"changed"});

	CHECK(r.exitCode == 2);
	CHECK(r.error == "strictor: not inside a git repository\n");
}

TEST_CASE("strictor prints usage and exits 2 for a malformed command line")
{
	TempDir tmp;

	for (std::vector<std::string> const& args : std::vector<std::vector<std::string>>{{}, {"frob"}, {"file"}, {"stdin"}, {"file", "-x", "a.cpp"}})
	{
		ProcessResult r = run(tmp, args);

		CHECK(r.exitCode == 2);
		CHECK(r.output.empty());
		CHECK_THAT(r.error, ContainsSubstring("usage: strictor file    [-p <build-dir>] [--whole] <path>..."));
	}
}
