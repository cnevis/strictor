#include "Clang/CompileDatabase.h"
#include "Clang/TranslationUnit.h"
#include "Support/Fixtures.h"
#include "Support/TempDir.h"
#include "Support/WorkingDir.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <string>
#include <vector>

static void database(TempDir const& tmp, std::string const& entries)
{
	tmp.write("compile_commands.json", "[" + entries + "]");
}

static std::string entry(TempDir const& tmp, std::string const& file, std::string const& command)
{
	return "{\"directory\": \"" + tmp.path().generic_string() + "\", \"file\": \"" + file + "\", \"command\": \"" + command + "\"}";
}

static bool has(std::vector<std::string> const& args, std::string const& arg)
{
	return std::find(args.begin(), args.end(), arg) != args.end();
}

static bool mentions(std::vector<std::string> const& args, std::string const& name)
{
	for (std::string const& a : args)
	{
		if (a.ends_with(name))
		{
			return true;
		}
	}

	return false;
}

TEST_CASE("CompileDatabase turns a gcc-style command into parse arguments")
{
	toolchain();
	TempDir tmp;
	database(tmp, entry(tmp, "src/a.cpp", "clang++ -Iinc -DY=2 -std=c++20 -c src/a.cpp -o a.o"));

	CompileDatabase db(tmp.path());
	CompileCommand cc = db.command(tmp.path() / "src" / "a.cpp");

	CHECK(std::filesystem::equivalent(cc.dir, tmp.path()));
	CHECK(has(cc.args, "-Iinc"));
	CHECK(has(cc.args, "-DY=2"));
	CHECK(has(cc.args, "-std=c++20"));
	CHECK_FALSE(has(cc.args, "clang++"));
	CHECK_FALSE(mentions(cc.args, "a.cpp"));
	CHECK(cc.args.back() == "-xc++");
}

TEST_CASE("CompileDatabase switches clang to cl mode for MSVC commands")
{
	toolchain();
	TempDir tmp;
	database(tmp,
	         entry(tmp, "a.cpp", "C:/VS/bin/cl.exe /nologo /Iinc /DY=2 /std:c++latest /c a.cpp") + "," +
	         entry(tmp, "b.c", "clang-cl.exe /Iinc /c b.c"));

	CompileDatabase db(tmp.path());
	CompileCommand cpp = db.command(tmp.path() / "a.cpp");

	CHECK(cpp.args.front() == "--driver-mode=cl");
	CHECK(has(cpp.args, "/Iinc"));
	CHECK(has(cpp.args, "/std:c++latest"));
	CHECK_FALSE(mentions(cpp.args, "a.cpp"));
	CHECK(cpp.args.back() == "/TP");

	CompileCommand c = db.command(tmp.path() / "b.c");

	CHECK(c.args.front() == "--driver-mode=cl");
	CHECK(c.args.back() == "/TC");
}

TEST_CASE("CompileDatabase infers commands for files it does not list")
{
	toolchain();
	TempDir tmp;
	database(tmp, entry(tmp, "src/a.cpp", "clang++ -Iinc -DY=2 -c src/a.cpp"));

	CompileDatabase db(tmp.path());
	CompileCommand header = db.command(tmp.path() / "src" / "a.h");

	CHECK(has(header.args, "-Iinc"));
	CHECK(has(header.args, "-DY=2"));
	CHECK_FALSE(has(header.args, "--"));
	CHECK_FALSE(mentions(header.args, "a.h"));
	CHECK_FALSE(mentions(header.args, "a.cpp"));
	CHECK(header.args.back() == "-xc++");

	CompileCommand c = db.command(tmp.path() / "src" / "other.c");

	CHECK(c.args.back() == "-xc");
}

TEST_CASE("CompileDatabase reads the arguments form")
{
	toolchain();
	TempDir tmp;
	tmp.write("compile_commands.json",
	          "[{\"directory\": \"" + tmp.path().generic_string() + "\", \"file\": \"a.cpp\", "
	          "\"arguments\": [\"clang++\", \"-DNAME=with space\", \"-c\", \"a.cpp\"]}]");

	CompileDatabase db(tmp.path());

	CHECK(has(db.command(tmp.path() / "a.cpp").args, "-DNAME=with space"));
}

TEST_CASE("CompileDatabase fails without compile_commands.json")
{
	toolchain();
	TempDir tmp;

	CHECK_THROWS_WITH(CompileDatabase(tmp.path()), Catch::Matchers::StartsWith("no compile_commands.json in"));
}

TEST_CASE("TranslationUnit resolves relative flags against the command directory")
{
	toolchain();
	TempDir project;
	TempDir elsewhere;
	project.write("inc/lib.h", "int lib(int);\n");
	database(project, entry(project, "src/a.cpp", "clang++ -Iinc -DY=2 -c src/a.cpp"));

	std::string code = "#include \"lib.h\"\nint f() { return lib(Y); }\n";
	CompileDatabase db(project.path());
	std::filesystem::path file = project.path() / "src" / "a.cpp";

	WorkingDir wd(elsewhere.path());

	CHECK_NOTHROW(TranslationUnit(file, code, db.command(file)));
	CHECK(std::filesystem::equivalent(std::filesystem::current_path(), elsewhere.path()));
	CHECK_NOTHROW(TranslationUnit(project.path() / "src" / "a.h", code, db.command(project.path() / "src" / "a.h")));
}
