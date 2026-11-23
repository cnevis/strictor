#include "Clang/ClangFormat.h"
#include "Support/Fixtures.h"
#include "Support/TempDir.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

TEST_CASE("ClangFormat formats with the nearest .clang-format")
{
	TempDir tmp;
	ClangFormat cf(toolchain().clangFormat());
	tmp.write(".clang-format", "BasedOnStyle: LLVM\n");
	tmp.write("allman/.clang-format", "BasedOnStyle: LLVM\nBreakBeforeBraces: Allman\nUseTab: Always\nIndentWidth: 4\nTabWidth: 4\n");

	std::string code = "int  f( ){return 0;}\n";

	CHECK(cf.format(tmp.path() / "a.cpp", code) == "int f() { return 0; }\n");
	CHECK(cf.format(tmp.path() / "deep" / "er" / "a.cpp", code) == "int f() { return 0; }\n");
	CHECK(cf.format(tmp.path() / "allman" / "a.cpp", "int f(){int x=0;return x;}\n") == "int f()\n{\n\tint x = 0;\n\treturn x;\n}\n");
	CHECK(cf.format(tmp.path() / "a.cpp", "") == "");
}

TEST_CASE("ClangFormat leaves files alone without a .clang-format")
{
	TempDir tmp;
	ClangFormat cf(toolchain().clangFormat());
	std::string code = "int  f( ){return 0;}\n";

	CHECK(cf.format(tmp.path() / "a.cpp", code) == code);
}

TEST_CASE("ClangFormat preserves line endings and UTF-8 text")
{
	TempDir tmp;
	ClangFormat cf(toolchain().clangFormat());
	tmp.write(".clang-format", "BasedOnStyle: LLVM\n");

	CHECK(cf.format(tmp.path() / "a.cpp", "int  a;\r\nint  b;\r\n") == "int a;\r\nint b;\r\n");
	CHECK(cf.format(tmp.path() / "a.cpp", "char const*  s = \"äö€\";\n") == "char const *s = \"äö€\";\n");
}

TEST_CASE("ClangFormat keeps namespace closer comments on the closing brace")
{
	TempDir tmp;
	ClangFormat cf(toolchain().clangFormat());
	tmp.write(".clang-format",
		"BasedOnStyle: Microsoft\n"
		"UseTab: Always\n"
		"TabWidth: 4\n"
		"IndentWidth: 4\n"
		"NamespaceIndentation: Inner\n"
		"Macros:\n"
		"  - \"NS_BEGIN=namespace lib {\"\n"
		"  - \"NS_END=}\"\n");

	std::string code =
		"NS_BEGIN\n"
		"\n"
		"namespace meta\n"
		"{\n"
		"\tint x;\n"
		"} // namespace meta\n"
		"\n"
		"NS_END\n"
		"\n"
		"#define G 1\n";

	std::string crlf;

	for (char c : code)
	{
		if (c == '\n')
		{
			crlf += '\r';
		}

		crlf += c;
	}

	CHECK(cf.format(tmp.path() / "a.h", code) == code);
	CHECK(cf.format(tmp.path() / "b.h", crlf) == crlf);
}

TEST_CASE("ClangFormat reports an invalid style file")
{
	TempDir tmp;
	ClangFormat cf(toolchain().clangFormat());
	tmp.write(".clang-format", "BasedOnStyle: NoSuchStyle\n");

	CHECK_THROWS_WITH(cf.format(tmp.path() / "a.cpp", "int a;\n"), Catch::Matchers::StartsWith("clang-format: "));
}
