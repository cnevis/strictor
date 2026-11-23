#include "Format/Formatter.h"
#include "Support/Fixtures.h"
#include "Support/TempDir.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <string>
#include <vector>

static std::vector<std::string> lines(std::vector<Diagnostic> const& ds)
{
	std::vector<std::string> result;

	for (Diagnostic const& d : ds)
	{
		result.push_back(std::to_string(d.line) + ":" + std::to_string(d.column) + " " + d.rule + ": " + d.message);
	}

	return result;
}

using Lines = std::vector<std::string>;

TEST_CASE("Formatter recognizes C and C++ sources by extension")
{
	Formatter f(toolchain(), {});

	for (char const* name : {"a.c", "a.cc", "a.cpp", "a.cxx", "a.h", "a.hh", "a.hpp", "a.hxx", "a.inl", "a.ixx", "A.CPP", "dir.x/b.H"})
	{
		INFO(name);
		CHECK(f.supports(name));
	}

	for (char const* name : {"a.txt", "a.cs", "a", "a.cpp.bak", ".strictor", "cpp"})
	{
		INFO(name);
		CHECK_FALSE(f.supports(name));
	}
}

TEST_CASE("Formatter formats, then checks names with cascading .strictor files")
{
	TempDir tmp;
	tmp.write(".clang-format", "BasedOnStyle: LLVM\n");
	tmp.write(".strictor",
	          "function { name: camelCase; }\n"
	          "variable[storage=local] { name: camelCase; }\n"
	          "* { banned-names: root; }\n");
	tmp.write("sub/.strictor", "variable[storage=local] { name: snake_case; }\n");

	Formatter f(toolchain(), {});
	std::string code = "int  Compute( ){int Bad_Name=0;int root=1;return Bad_Name+root;}\n";

	FormatResult top = f.format(tmp.path() / "a.cpp", code);

	CHECK(top.text == "int Compute() {\n  int Bad_Name = 0;\n  int root = 1;\n  return Bad_Name + root;\n}\n");
	CHECK(lines(top.diagnostics) == Lines{
		"1:5 naming.case: function 'Compute' should be camelCase",
		"2:7 naming.case: local 'Bad_Name' should be camelCase",
		"3:7 naming.banned: local 'root' is a banned name"});

	FormatResult sub = f.format(tmp.path() / "sub" / "a.cpp", code);

	CHECK(lines(sub.diagnostics) == Lines{
		"1:5 naming.case: function 'Compute' should be camelCase",
		"2:7 naming.case: local 'Bad_Name' should be snake_case",
		"3:7 naming.banned: local 'root' is a banned name"});
}

TEST_CASE("Formatter sorts diagnostics by position")
{
	TempDir tmp;
	tmp.write(".strictor", "macro { name: UPPER_SNAKE; }\n* { banned-parts: Bad; }\n");

	Formatter f(toolchain(), {});
	FormatResult r = f.format(tmp.path() / "a.cpp", "int badB, badA;\n#define badMacro 1\n");

	CHECK(lines(r.diagnostics) == Lines{
		"1:5 naming.banned: global 'badB' contains banned part 'Bad'",
		"1:11 naming.banned: global 'badA' contains banned part 'Bad'",
		"2:9 naming.case: macro 'badMacro' should be UPPER_SNAKE",
		"2:9 naming.banned: macro 'badMacro' contains banned part 'Bad'"});
}

TEST_CASE("Formatter checks nothing without a .strictor and keeps text without a .clang-format")
{
	TempDir tmp;
	Formatter f(toolchain(), {});
	std::string code = "int  Bad_Name( ){return 0;}\n";

	FormatResult r = f.format(tmp.path() / "a.cpp", code);

	CHECK(r.text == code);
	CHECK(r.diagnostics.empty());
}

TEST_CASE("Formatter fails on a broken .strictor or code that does not parse")
{
	TempDir tmp;
	Formatter f(toolchain(), {});
	tmp.write("broken/.strictor", "type {\n\tcolour: red;\n}\n");

	CHECK_THROWS_WITH(f.format(tmp.path() / "broken" / "a.cpp", "int a;\n"),
	                  Catch::Matchers::EndsWith(".strictor:2: unknown property 'colour'"));
	CHECK_THROWS_WITH(f.format(tmp.path() / "a.cpp", "int a = nope;\n"),
	                  Catch::Matchers::StartsWith("parse failed at 1:9"));
}

TEST_CASE("Formatter finds compile_commands.json above the file or in the build directory")
{
	TempDir tmp;
	std::string dir = tmp.path().generic_string();
	std::string db = "[{\"directory\": \"" + dir + "\", \"file\": \"src/a.cpp\", \"command\": \"clang++ -Iinc -DY=2 -c src/a.cpp\"}]";
	std::string code = "#include \"lib.h\"\nint f() { return lib(Y); }\n";
	tmp.write("inc/lib.h", "int lib(int);\n");

	{
		Formatter f(toolchain(), {});
		CHECK_THROWS(f.format(tmp.path() / "src" / "a.cpp", code));
	}

	tmp.write("build/compile_commands.json", db);

	{
		Formatter f(toolchain(), tmp.path() / "build");
		CHECK_NOTHROW(f.format(tmp.path() / "src" / "a.cpp", code));
		CHECK_NOTHROW(f.format(tmp.path() / "src" / "a.h", code));
	}

	{
		Formatter f(toolchain(), tmp.path() / "inc");
		CHECK_THROWS_WITH(f.format(tmp.path() / "src" / "a.cpp", code), Catch::Matchers::StartsWith("no compile_commands.json in"));
	}

	tmp.write("compile_commands.json", db);

	{
		Formatter f(toolchain(), {});
		CHECK_NOTHROW(f.format(tmp.path() / "src" / "a.cpp", code));
	}
}

static char const g_Layout[] =
	"control                { blank-before: 1; blank-after: 1; }\n"
	"control[braced=no]     { blank-after: 0; }\n"
	"control > condition    { max-clauses: 3; }\n"
	"expression             { logical-operators: words; }\n";

static char const g_Allman[] =
	"BasedOnStyle: LLVM\n"
	"BreakBeforeBraces: Allman\n"
	"UseTab: Always\n"
	"IndentWidth: 4\n"
	"TabWidth: 4\n"
	"AllowShortIfStatementsOnASingleLine: Never\n"
	"AllowShortLoopsOnASingleLine: false\n"
	"AllowShortBlocksOnASingleLine: Never\n";

TEST_CASE("Formatter fixes blank lines and logical operators, then reports long conditions")
{
	TempDir tmp;
	tmp.write(".clang-format", g_Allman);
	tmp.write(".strictor", g_Layout);

	std::string code =
		"int f(int a, int b)\n"
		"{\n"
		"\tint x = a;\n"
		"\tif (a && !(b > 1))\n"
		"\t{\n"
		"\t\tx++;\n"
		"\t}\n"
		"\tx += 2;\n"
		"\tfor (int i = 0; i < 3; i++)\n"
		"\t\tx += i;\n"
		"\tx--;\n"
		"\n"
		"\n"
		"\twhile (a > 0 && b > 0 || a < -5 && b < -5)\n"
		"\t{\n"
		"\t\ta--;\n"
		"\t}\n"
		"\treturn x;\n"
		"}\n";

	std::string expected =
		"int f(int a, int b)\n"
		"{\n"
		"\tint x = a;\n"
		"\n"
		"\tif (a and not (b > 1))\n"
		"\t{\n"
		"\t\tx++;\n"
		"\t}\n"
		"\n"
		"\tx += 2;\n"
		"\n"
		"\tfor (int i = 0; i < 3; i++)\n"
		"\t\tx += i;\n"
		"\tx--;\n"
		"\n"
		"\twhile (a > 0 and b > 0 or a < -5 and b < -5)\n"
		"\t{\n"
		"\t\ta--;\n"
		"\t}\n"
		"\n"
		"\treturn x;\n"
		"}\n";

	Formatter f(toolchain(), {});
	FormatResult r = f.format(tmp.path() / "a.cpp", code);

	CHECK(r.text == expected);
	CHECK(lines(r.diagnostics) == Lines{"16:9 layout.condition: condition has 4 clauses; split into guard clauses (max 3)"});

	FormatResult again = f.format(tmp.path() / "a.cpp", r.text);

	CHECK(again.text == expected);
	CHECK(lines(again.diagnostics) == lines(r.diagnostics));
}

TEST_CASE("Formatter switches operators back to symbols and leaves nested blocks alone")
{
	TempDir tmp;
	tmp.write(".clang-format", g_Allman);
	tmp.write(".strictor", "expression { logical-operators: symbols; }\ncontrol { blank-after: 1; }\n");

	Formatter f(toolchain(), {});
	FormatResult r = f.format(tmp.path() / "a.cpp",
		"bool f(bool a, bool b)\n"
		"{\n"
		"\tif (a)\n"
		"\t{\n"
		"\t\tif (b)\n"
		"\t\t{\n"
		"\t\t\treturn not a;\n"
		"\t\t}\n"
		"\t}\n"
		"\treturn a and b or not b;\n"
		"}\n");

	CHECK(r.text ==
		"bool f(bool a, bool b)\n"
		"{\n"
		"\tif (a)\n"
		"\t{\n"
		"\t\tif (b)\n"
		"\t\t{\n"
		"\t\t\treturn !a;\n"
		"\t\t}\n"
		"\t}\n"
		"\n"
		"\treturn a && b || !b;\n"
		"}\n");
	CHECK(r.diagnostics.empty());
}

TEST_CASE("Formatter runs Lua scripts named by .strictor on matching nodes")
{
	TempDir tmp;
	tmp.write(".clang-format", g_Allman);
	tmp.write("lua/rules.lua",
		"on('function', function(node, r)\n"
		"\tif node:lines() > 5 then\n"
		"\t\tr:report('layout.long-function', node, 'function has ' .. node:lines() .. ' lines')\n"
		"\tend\n"
		"end)\n"
		"on('return', function(node, r)\n"
		"\tif node.parent.type == 'case' and node:blankBefore() == 0 then\n"
		"\t\tr:insertBefore(node, '\\n')\n"
		"\tend\n"
		"end)\n");
	tmp.write(".strictor", "* { script: lua/rules.lua; }\n");

	std::string code =
		"int pick(int v)\n"
		"{\n"
		"\tswitch (v)\n"
		"\t{\n"
		"\tcase 1:\n"
		"\t\treturn 3;\n"
		"\t}\n"
		"\treturn 0;\n"
		"}\n";

	Formatter f(toolchain(), {});
	FormatResult r = f.format(tmp.path() / "a.cpp", code);

	CHECK(r.text ==
		"int pick(int v)\n"
		"{\n"
		"\tswitch (v)\n"
		"\t{\n"
		"\tcase 1:\n"
		"\n"
		"\t\treturn 3;\n"
		"\t}\n"
		"\treturn 0;\n"
		"}\n");
	CHECK(lines(r.diagnostics) == Lines{"1:5 layout.long-function: function has 10 lines"});
}

TEST_CASE("Formatter turns a script for a subtree off with a nearer .strictor")
{
	TempDir tmp;
	tmp.write("rules.lua", "on('function', function(node, r) r:report('test.seen', node, node.name) end)\n");
	tmp.write(".strictor", "* { script: rules.lua; }\n");
	tmp.write("quiet/.strictor", "function { script: none.lua; }\n");
	tmp.write("quiet/none.lua", "");

	Formatter f(toolchain(), {});

	CHECK(lines(f.format(tmp.path() / "a.cpp", "int f() { return 0; }\n").diagnostics) == Lines{"1:5 test.seen: f"});
	CHECK(f.format(tmp.path() / "quiet" / "a.cpp", "int f() { return 0; }\n").diagnostics.empty());
}

TEST_CASE("Formatter reports script errors")
{
	TempDir tmp;
	tmp.write("rules.lua", "on('function', function(node, r) error('bad rule') end)\n");
	tmp.write(".strictor", "* { script: rules.lua; }\n");

	Formatter f(toolchain(), {});

	CHECK_THROWS_WITH(f.format(tmp.path() / "a.cpp", "int f() { return 0; }\n"), Catch::Matchers::ContainsSubstring("rules.lua:1: bad rule"));
}

TEST_CASE("Formatter reports helpers by the attribute rules and excuses utility folders")
{
	TempDir tmp;
	tmp.write(".strictor",
		"function[scope=free][linkage=internal][file=class]  { helper: report; }\n"
		"function[scope=free][callers=one-class]             { helper: report; }\n"
		"function[scope=free][types=plain]                   { helper: allow; }\n"
		"function[scope=free][callers=several]               { helper: allow; }\n"
		"function[scope=free]                                { helper-folders: System Math; helper-allow: lerp; }\n");

	std::string code =
		"struct Point { int x; };\n"
		"class Widget\n"
		"{\n"
		"public:\n"
		"\tint area() const;\n"
		"\tPoint corner() const;\n"
		"};\n"
		"static bool upper(char c) { return c >= 'A'; }\n"
		"static Point shift(Point p) { return p; }\n"
		"int lerp(Point a) { return a.x; }\n"
		"int place(Point a) { return a.x; }\n"
		"int Widget::area() const { return upper('a') + lerp({1}) + place({2}); }\n"
		"Point Widget::corner() const { return shift({1}); }\n";

	Formatter f(toolchain(), {});

	CHECK(lines(f.format(tmp.path() / "Widget.cpp", code).diagnostics) == Lines{
		"8:13 design.helper: free function 'upper' belongs in class 'Widget' (static in its .cpp, called only by Widget)",
		"9:14 design.helper: free function 'shift' belongs in class 'Widget' (static in its .cpp, called only by Widget)",
		"11:5 design.helper: free function 'place' belongs in class 'Widget' (called only by Widget)"});

	CHECK(f.format(tmp.path() / "Math" / "Widget.cpp", code).diagnostics.empty());

	tmp.write("plain/.strictor", "function[scope=free][linkage=internal][file=class][types=plain] { helper: allow; }\n");

	CHECK(lines(f.format(tmp.path() / "plain" / "Widget.cpp", code).diagnostics) == Lines{
		"9:14 design.helper: free function 'shift' belongs in class 'Widget' (static in its .cpp, called only by Widget)",
		"11:5 design.helper: free function 'place' belongs in class 'Widget' (called only by Widget)"});
}

TEST_CASE("Formatter fixes operators in macro arguments and spaces around macro statements")
{
	TempDir tmp;
	tmp.write(".clang-format", g_Allman);
	tmp.write(".strictor", g_Layout);

	std::string code =
		"#define VERIFY(x) (void)(x)\n"
		"#define TRY(x) (void)(x)\n"
		"int run(bool a, bool b)\n"
		"{\n"
		"\tTRY(a);\n"
		"\tif (b)\n"
		"\t{\n"
		"\t\treturn 2;\n"
		"\t}\n"
		"\tVERIFY(a && !b);\n"
		"\treturn 0;\n"
		"}\n";

	Formatter f(toolchain(), {});
	FormatResult r = f.format(tmp.path() / "a.cpp", code);

	CHECK(r.text ==
		"#define VERIFY(x) (void)(x)\n"
		"#define TRY(x) (void)(x)\n"
		"int run(bool a, bool b)\n"
		"{\n"
		"\tTRY(a);\n"
		"\n"
		"\tif (b)\n"
		"\t{\n"
		"\t\treturn 2;\n"
		"\t}\n"
		"\n"
		"\tVERIFY(a and not b);\n"
		"\treturn 0;\n"
		"}\n");
}

TEST_CASE("Formatter splits declarators, braces case bodies and reports plain enums and plural types")
{
	TempDir tmp;
	tmp.write(".clang-format", g_Allman);
	tmp.write(".strictor",
		"type                   { plural: report; plural-allow: Options; }\n"
		"type[storage=template] { plural: allow; }\n"
		"type[enum=plain]       { report: use enum class; }\n"
		"variable               { declarators: split; }\n"
		"case                   { braces: insert; }\n");

	std::string code =
		"enum Colors { Red, Green };\n"
		"struct Point\n"
		"{\n"
		"\tfloat x, y;\n"
		"};\n"
		"template <typename... Args> int sum(Args... args) { return (0 + ... + args); }\n"
		"int f(int v)\n"
		"{\n"
		"\tint a = 1, *b = &a;\n"
		"\tfor (int i = 0, j = 2; i < j; i++)\n"
		"\t{\n"
		"\t\ta++;\n"
		"\t}\n"
		"\tswitch (v)\n"
		"\t{\n"
		"\tcase 0: a = 2; break;\n"
		"\tcase 1: { a = 3; break; }\n"
		"\tdefault: a = 4;\n"
		"\t}\n"
		"\treturn a + *b;\n"
		"}\n";

	Formatter f(toolchain(), {});
	FormatResult r = f.format(tmp.path() / "a.cpp", code);

	std::string expected =
		"enum Colors\n"
		"{\n"
		"\tRed,\n"
		"\tGreen\n"
		"};\n"
		"struct Point\n"
		"{\n"
		"\tfloat x;\n"
		"\tfloat y;\n"
		"};\n"
		"template <typename... Args> int sum(Args... args) { return (0 + ... + args); }\n"
		"int f(int v)\n"
		"{\n"
		"\tint a = 1;\n"
		"\tint *b = &a;\n"
		"\tfor (int i = 0, j = 2; i < j; i++)\n"
		"\t{\n"
		"\t\ta++;\n"
		"\t}\n"
		"\tswitch (v)\n"
		"\t{\n"
		"\tcase 0:\n"
		"\t{\n"
		"\t\ta = 2;\n"
		"\t\tbreak;\n"
		"\t}\n"
		"\tcase 1:\n"
		"\t{\n"
		"\t\ta = 3;\n"
		"\t\tbreak;\n"
		"\t}\n"
		"\tdefault:\n"
		"\t{\n"
		"\t\ta = 4;\n"
		"\t}\n"
		"\t}\n"
		"\treturn a + *b;\n"
		"}\n";

	Lines found = {"1:6 naming.plural: type 'Colors' should be singular", "1:6 report: use enum class"};

	CHECK(r.text == expected);
	CHECK(lines(r.diagnostics) == found);

	FormatResult again = f.format(tmp.path() / "a.cpp", r.text);

	CHECK(again.text == expected);
	CHECK(lines(again.diagnostics) == found);
}

TEST_CASE("Formatter puts a blank line after multi-line functions in classes, namespaces and files")
{
	TempDir tmp;
	tmp.write(".clang-format", "DisableFormat: true\n");
	tmp.write(".strictor", "function[lines=multi] { blank-after: 1; }\n");

	std::string code =
		"struct Paint;\n"
		"namespace impl\n"
		"{\n"
		"\tstruct Parser\n"
		"\t{\n"
		"\t\tstatic bool parseColor(\n"
		"\t\t\tchar const* text,\n"
		"\t\t\tPaint* result);\n"
		"\t\tstatic bool parsePaint(\n"
		"\t\t\tchar const* text,\n"
		"\t\t\tPaint* result);\n"
		"\t\tstatic bool parseUrl(char const* text);\n"
		"\t\tstatic bool parseName(\n"
		"\t\t\tchar const* text);\n"
		"\t};\n"
		"\tbool check(\n"
		"\t\tint v);\n"
		"\tbool other(int v);\n"
		"} // namespace impl\n"
		"int top(\n"
		"\tint v);\n"
		"int last(int v);\n";

	Formatter f(toolchain(), {});
	FormatResult r = f.format(tmp.path() / "a.h", code);

	std::string expected =
		"struct Paint;\n"
		"namespace impl\n"
		"{\n"
		"\tstruct Parser\n"
		"\t{\n"
		"\t\tstatic bool parseColor(\n"
		"\t\t\tchar const* text,\n"
		"\t\t\tPaint* result);\n"
		"\n"
		"\t\tstatic bool parsePaint(\n"
		"\t\t\tchar const* text,\n"
		"\t\t\tPaint* result);\n"
		"\n"
		"\t\tstatic bool parseUrl(char const* text);\n"
		"\t\tstatic bool parseName(\n"
		"\t\t\tchar const* text);\n"
		"\t};\n"
		"\tbool check(\n"
		"\t\tint v);\n"
		"\n"
		"\tbool other(int v);\n"
		"} // namespace impl\n"
		"int top(\n"
		"\tint v);\n"
		"\n"
		"int last(int v);\n";

	CHECK(r.text == expected);
	CHECK(f.format(tmp.path() / "a.h", r.text).text == expected);
}
