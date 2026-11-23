#include "Clang/TranslationUnit.h"
#include "Support/Fixtures.h"
#include "Support/TempDir.h"
#include "System/Text.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <map>
#include <string>
#include <vector>

static SyntaxTree parse(TempDir const& tmp, std::string const& code)
{
	toolchain();
	CompileCommand cmd = {tmp.path(), {"-xc++", "-std=c++2c"}};
	TranslationUnit tu(tmp.path() / "a.cpp", code, cmd);

	return tu.tree();
}

static std::vector<SyntaxNode const*> named(SyntaxTree const& tree)
{
	std::vector<SyntaxNode const*> result;

	for (std::unique_ptr<SyntaxNode> const& n : tree.nodes())
	{
		if (n->checkName)
		{
			result.push_back(n.get());
		}
	}

	return result;
}

static int count(std::vector<SyntaxNode const*> const& ss, std::string const& name)
{
	int n = 0;

	for (SyntaxNode const* s : ss)
	{
		if (s->name == name)
		{
			n++;
		}
	}

	return n;
}

static SyntaxNode const* find(std::vector<SyntaxNode const*> const& ss, std::string const& name)
{
	for (SyntaxNode const* s : ss)
	{
		if (s->name == name)
		{
			return s;
		}
	}

	return nullptr;
}

using Attributes = std::map<std::string, std::string>;

static char const g_Code[] =
	"#define LIMIT 4\n"
	"#define twice(x) ((x) * 2)\n"
	"\n"
	"namespace app\n"
	"{\n"
	"\tint g_Count = 0;\n"
	"\tconstexpr int MAX_ITEMS = 8;\n"
	"\tint const kSeed = 1;\n"
	"\n"
	"\tenum class Mode\n"
	"\t{\n"
	"\t\tFAST,\n"
	"\t\tSLOW\n"
	"\t};\n"
	"\n"
	"\tusing Size = unsigned;\n"
	"\ttypedef int Handle;\n"
	"\n"
	"\ttemplate <typename T, int N>\n"
	"\tstruct Buffer\n"
	"\t{\n"
	"\t\tT Items[N];\n"
	"\t};\n"
	"\n"
	"\ttemplate <typename U>\n"
	"\tconcept Small = sizeof(U) < 8;\n"
	"\n"
	"\tclass Widget\n"
	"\t{\n"
	"\tpublic:\n"
	"\t\tWidget();\n"
	"\t\t~Widget();\n"
	"\t\tvirtual void paint();\n"
	"\t\tint width() const;\n"
	"\t\tWidget& operator=(Widget const& other);\n"
	"\t\toperator bool() const;\n"
	"\t\tstatic int s_Instances;\n"
	"\n"
	"\tprotected:\n"
	"\t\tint m_Height = 0;\n"
	"\n"
	"\tprivate:\n"
	"\t\tint m_Width = 0;\n"
	"\t\tvoid layout();\n"
	"\t};\n"
	"\n"
	"\tclass Button : public Widget\n"
	"\t{\n"
	"\tpublic:\n"
	"\t\tvoid paint() override;\n"
	"\t};\n"
	"\n"
	"\tclass Forward;\n"
	"\tclass Forward;\n"
	"\n"
	"\tint Widget::s_Instances = 0;\n"
	"\n"
	"\tint Widget::width() const\n"
	"\t{\n"
	"\t\tstatic int calls = 0;\n"
	"\t\tint const local = m_Width;\n"
	"\n"
	"\t\treturn local + calls;\n"
	"\t}\n"
	"\n"
	"\tint compute(int value, int)\n"
	"\t{\n"
	"\t\tauto twiceIt = [](int v) { return v * 2; };\n"
	"\n"
	"\t\treturn twiceIt(value);\n"
	"\t}\n"
	"}\n";

TEST_CASE("TranslationUnit collects declarations with their node type and attributes")
{
	TempDir tmp;
	SyntaxTree tree = parse(tmp, g_Code);
	std::vector<SyntaxNode const*> ss = named(tree);

	struct Expected
	{
		char const* name;
		char const* type;
		Attributes attributes;
	};

	Expected const expected[] =
	{
		{"LIMIT", "macro", {}},
		{"twice", "macro", {}},
		{"app", "namespace", {}},
		{"g_Count", "variable", {{"storage", "global"}, {"const", "none"}}},
		{"MAX_ITEMS", "variable", {{"storage", "global"}, {"const", "constexpr"}}},
		{"kSeed", "variable", {{"storage", "global"}, {"const", "const"}}},
		{"Mode", "type", {{"enum", "class"}}},
		{"FAST", "enumerator", {}},
		{"SLOW", "enumerator", {}},
		{"Size", "type", {}},
		{"Handle", "type", {}},
		{"T", "type", {{"storage", "template"}}},
		{"N", "variable", {{"storage", "template"}, {"const", "none"}}},
		{"Buffer", "type", {}},
		{"Items", "variable", {{"storage", "member"}, {"access", "public"}, {"const", "none"}}},
		{"U", "type", {{"storage", "template"}}},
		{"Small", "type", {}},
		{"Widget", "type", {}},
		{"paint", "function", {{"access", "public"}, {"scope", "member"}}},
		{"width", "function", {{"access", "public"}, {"scope", "member"}}},
		{"other", "variable", {{"storage", "parameter"}, {"const", "none"}}},
		{"s_Instances", "variable", {{"storage", "member"}, {"access", "public"}, {"const", "none"}}},
		{"m_Height", "variable", {{"storage", "member"}, {"access", "protected"}, {"const", "none"}}},
		{"m_Width", "variable", {{"storage", "member"}, {"access", "private"}, {"const", "none"}}},
		{"layout", "function", {{"access", "private"}, {"scope", "member"}}},
		{"Button", "type", {}},
		{"calls", "variable", {{"storage", "local"}, {"const", "none"}}},
		{"local", "variable", {{"storage", "local"}, {"const", "const"}}},
		{"compute", "function", {{"scope", "free"}, {"linkage", "external"}, {"types", "plain"}, {"file", "class"}, {"callers", "none"}}},
		{"value", "variable", {{"storage", "parameter"}, {"const", "none"}}},
		{"twiceIt", "variable", {{"storage", "local"}, {"const", "none"}}},
		{"v", "variable", {{"storage", "parameter"}, {"const", "none"}}}
	};

	for (Expected const& e : expected)
	{
		INFO(e.name);
		CHECK(count(ss, e.name) == 1);

		SyntaxNode const* s = find(ss, e.name);
		REQUIRE(s);
		CHECK(s->node.type == e.type);
		CHECK(s->node.attributes == e.attributes);
	}

	CHECK(ss.size() == std::size(expected));

	SyntaxNode const* global = find(ss, "g_Count");
	SyntaxNode const* macro = find(ss, "LIMIT");
	REQUIRE(global);
	REQUIRE(macro);

	CHECK(global->line == 6);
	CHECK(global->column == 6);
	CHECK(macro->line == 1);
	CHECK(macro->column == 9);
}

TEST_CASE("TranslationUnit leaves out declarations from other files and builtins")
{
	TempDir tmp;
	tmp.write("lib.h", "#define LIB_MACRO 1\nint libFunction();\nstruct LibType {};\n");

	SyntaxTree tree = parse(tmp, "#include \"lib.h\"\nint mine() { return libFunction() + LIB_MACRO; }\n");
	std::vector<SyntaxNode const*> ss = named(tree);

	REQUIRE(ss.size() == 1);
	CHECK(ss[0]->name == "mine");
	CHECK(count(ss, "__cplusplus") == 0);
}

TEST_CASE("TranslationUnit parses a header once when a forced include already pulls it in")
{
	TempDir tmp;
	toolchain();
	tmp.write("pre.h", "#include \"once.h\"\n#include \"guard.h\"\n");
	tmp.write("once.h", "#pragma once\nstruct OnDisk {};\n");
	tmp.write("guard.h", "#ifndef GUARD_H\n#define GUARD_H\nstruct OnDisk {};\n#endif\n");

	CompileCommand cmd = {tmp.path(), {"-xc++", "-std=c++2c", "-include", utf8(tmp.path() / "pre.h")}};
	TranslationUnit once(tmp.path() / "once.h", "#pragma once\n#define ONCE_MACRO 1\nstruct Edited {};\n", cmd);
	TranslationUnit guard(tmp.path() / "guard.h", "#ifndef GUARD_H\n#define GUARD_H\nstruct Guarded {};\n#endif\n", cmd);
	SyntaxTree onceTree = once.tree();
	SyntaxTree guardTree = guard.tree();
	std::vector<SyntaxNode const*> a = named(onceTree);
	std::vector<SyntaxNode const*> b = named(guardTree);

	CHECK(count(a, "Edited") == 1);
	CHECK(count(a, "ONCE_MACRO") == 1);
	CHECK(count(a, "OnDisk") == 0);
	CHECK(count(b, "Guarded") == 1);
	CHECK(count(b, "GUARD_H") == 1);
}

TEST_CASE("TranslationUnit fails with the location of the first error")
{
	TempDir tmp;

	CHECK_THROWS_WITH(parse(tmp, "int f()\n{\n\treturn nope;\n}\n"),
	                  "parse failed at 3:9: use of undeclared identifier 'nope'");

	tmp.write("bad.h", "int broken = ;\nint alsoBroken() { return nope; }\n");
	tmp.write("lost.h", "#include \"gone.h\"\n");

	SyntaxTree tree = parse(tmp, "#include \"bad.h\"\nint mine() { return 0; }\n");

	CHECK(count(named(tree), "mine") == 1);
	CHECK(count(named(tree), "alsoBroken") == 0);

	CHECK_THROWS_WITH(parse(tmp, "#include \"bad.h\"\nint mine() { return nope; }\n"),
	                  "parse failed at 2:21: use of undeclared identifier 'nope'");

	CHECK_THROWS_WITH(parse(tmp, "#include \"missing.h\"\n"),
	                  "parse failed at 1:10: 'missing.h' file not found");

	CHECK_THROWS_WITH(parse(tmp, "#include \"lost.h\"\nint mine() { return 0; }\n"),
	                  Catch::Matchers::ContainsSubstring("lost.h:1:10: 'gone.h' file not found"));

	CHECK_NOTHROW(parse(tmp, "int unused() { int x; return 0; }\n"));
}

static std::vector<std::string> types(SyntaxNode const* n)
{
	std::vector<std::string> result;

	for (SyntaxNode const* c : n->children)
	{
		result.push_back(c->node.type);
	}

	return result;
}

static std::vector<SyntaxNode const*> all(SyntaxTree const& tree, std::string const& type)
{
	std::vector<SyntaxNode const*> result;

	for (std::unique_ptr<SyntaxNode> const& n : tree.nodes())
	{
		if (n->node.type == type)
		{
			result.push_back(n.get());
		}
	}

	return result;
}

static char const g_Statements[] =
	"#define CHECKED(x) if (x) { }\n"
	"int f(int a, int b)\n"
	"{\n"
	"\tint x = a;\n"
	"\tif (a && !b)\n"
	"\t{\n"
	"\t\tx++;\n"
	"\t}\n"
	"\tfor (int i = 0; i < a and i > -1; i++)\n"
	"\t\tx += i;\n"
	"\twhile (a > 0 || b > 0 && a < b)\n"
	"\t{\n"
	"\t\ta--;\n"
	"\t}\n"
	"\tdo\n"
	"\t{\n"
	"\t\tx--;\n"
	"\t} while (x > 0);\n"
	"\tswitch (x)\n"
	"\t{\n"
	"\t\tcase 1:\n"
	"\t\t\treturn 3;\n"
	"\t\tdefault:\n"
	"\t\t\tbreak;\n"
	"\t}\n"
	"\ttry { } catch (...) { }\n"
	"\tCHECKED(a)\n"
	"\treturn x;\n"
	"}\n";

TEST_CASE("TranslationUnit builds a statement tree with controls, conditions and logical operators")
{
	TempDir tmp;
	SyntaxTree tree = parse(tmp, g_Statements);

	std::vector<SyntaxNode const*> functions = all(tree, "function");
	REQUIRE(functions.size() == 1);
	CHECK(types(functions[0]) == std::vector<std::string>{"variable", "variable", "block"});

	SyntaxNode const* body = functions[0]->children[2];
	CHECK(body->firstLine == 3);
	CHECK(body->lastLine == 29);
	CHECK(types(body) == std::vector<std::string>{"statement", "control", "control", "control", "control", "control", "control", "statement", "return"});

	struct Expected
	{
		char const* keyword;
		char const* braced;
		int first;
		int last;
		int clauses;
	};

	Expected const expected[] =
	{
		{"if", "yes", 5, 8, 2},
		{"for", "no", 9, 10, 2},
		{"while", "yes", 11, 14, 3},
		{"do", "yes", 15, 18, 1},
		{"switch", "yes", 19, 25, 1},
		{"try", "yes", 26, 26, 0}
	};

	for (std::size_t i = 0; i < std::size(expected); i++)
	{
		Expected const& e = expected[i];
		SyntaxNode const* control = body->children[i + 1];
		INFO(e.keyword);

		CHECK(control->node.attributes == Attributes{{"keyword", e.keyword}, {"braced", e.braced}});
		CHECK(control->firstLine == e.first);
		CHECK(control->lastLine == e.last);

		int conditions = 0;

		for (SyntaxNode const* c : control->children)
		{
			if (c->node.type == "condition")
			{
				conditions++;
				CHECK(c->clauses == e.clauses);
				CHECK(c->node.parent == &control->node);
			}
		}

		CHECK(conditions == (e.clauses > 0 ? 1 : 0));
	}

	std::vector<SyntaxNode const*> expressions = all(tree, "expression");
	std::vector<std::string> spellings;

	for (SyntaxNode const* e : expressions)
	{
		spellings.push_back(e->operatorSpelling);
		CHECK(std::string(g_Statements).substr(e->operatorOffset, e->operatorSpelling.size()) == e->operatorSpelling);
	}

	CHECK(spellings == std::vector<std::string>{"&&", "!", "and", "||", "&&"});
	CHECK(expressions[0]->parent->node.type == "condition");
	CHECK(expressions[1]->parent == expressions[0]);

	std::vector<SyntaxNode const*> cases = all(tree, "case");
	REQUIRE(cases.size() == 2);
	CHECK(types(cases[0]) == std::vector<std::string>{"return"});
	CHECK(cases[0]->parent->node.type == "block");

	CHECK(all(tree, "return").size() == 2);
	CHECK(body->children.back()->firstLine == 28);
}

static char const g_Functions[] =
	"struct Point { int x; };\n"
	"class Widget\n"
	"{\n"
	"public:\n"
	"\tint area() const;\n"
	"\tint size() const { return twiceInline(1); }\n"
	"\tstatic int twiceInline(int v) { return v * 2; }\n"
	"};\n"
	"class Other\n"
	"{\n"
	"public:\n"
	"\tint run() const;\n"
	"};\n"
	"namespace\n"
	"{\n"
	"\tint hidden(int v) { return v; }\n"
	"}\n"
	"static bool upper(char c) { return c >= 'A'; }\n"
	"static int shared(int v) { return v; }\n"
	"static Point shift(Point p) { return p; }\n"
	"int unused(Point const* p) { return p->x; }\n"
	"int recursive(int n) { return n > 0 ? recursive(n - 1) : 0; }\n"
	"template <typename T> T identity(T v) { return v; }\n"
	"int (*pointer)(int) = shared;\n"
	"int Widget::area() const { return upper('a') + shift({1}).x + hidden(2) + identity(3); }\n"
	"int Other::run() const { return shared(1); }\n";

TEST_CASE("TranslationUnit computes scope, linkage, types, callers and owner for functions")
{
	TempDir tmp;
	SyntaxTree tree = parse(tmp, g_Functions);
	std::vector<SyntaxNode const*> fs = all(tree, "function");

	struct Expected
	{
		char const* name;
		Attributes attributes;
		char const* owner;
	};

	Expected const expected[] =
	{
		{"area", {{"access", "public"}, {"scope", "member"}}, ""},
		{"size", {{"access", "public"}, {"scope", "member"}}, ""},
		{"twiceInline", {{"access", "public"}, {"scope", "member"}}, ""},
		{"run", {{"access", "public"}, {"scope", "member"}}, ""},
		{"hidden", {{"scope", "free"}, {"linkage", "internal"}, {"types", "plain"}, {"file", "class"}, {"callers", "one-class"}}, "Widget"},
		{"upper", {{"scope", "free"}, {"linkage", "internal"}, {"types", "plain"}, {"file", "class"}, {"callers", "one-class"}}, "Widget"},
		{"shared", {{"scope", "free"}, {"linkage", "internal"}, {"types", "plain"}, {"file", "class"}, {"callers", "several"}}, "Widget"},
		{"shift", {{"scope", "free"}, {"linkage", "internal"}, {"types", "project"}, {"file", "class"}, {"callers", "one-class"}}, "Widget"},
		{"unused", {{"scope", "free"}, {"linkage", "external"}, {"types", "project"}, {"file", "class"}, {"callers", "none"}}, "Widget"},
		{"recursive", {{"scope", "free"}, {"linkage", "external"}, {"types", "plain"}, {"file", "class"}, {"callers", "none"}}, "Widget"},
		{"identity", {{"scope", "free"}, {"linkage", "external"}, {"types", "plain"}, {"file", "class"}, {"callers", "one-class"}}, "Widget"}
	};

	for (Expected const& e : expected)
	{
		INFO(e.name);
		SyntaxNode const* f = find(fs, e.name);
		REQUIRE(f);
		CHECK(f->node.attributes == e.attributes);
		CHECK(f->owner == e.owner);
	}
}

TEST_CASE("TranslationUnit marks free functions in files without member definitions")
{
	TempDir tmp;
	SyntaxTree tree = parse(tmp, "static int one(int v) { return v; }\nint two() { return one(2); }\nstruct Plain { int v; };\n");
	std::vector<SyntaxNode const*> fs = all(tree, "function");

	REQUIRE(fs.size() == 2);
	CHECK(fs[0]->node.attributes.at("file") == "other");
	CHECK(fs[0]->node.attributes.at("callers") == "several");
	CHECK(fs[0]->owner.empty());
	CHECK(fs[1]->node.attributes.at("callers") == "none");
}

static char const g_Macros[] =
	"#define NS_BEGIN namespace lib { inline namespace v1 {\n"
	"#define NS_END } }\n"
	"#define VERIFY(x) (void)(x)\n"
	"#define TRY(x) if (!(x)) { return 1; }\n"
	"struct lua_State;\n"
	"struct Later;\n"
	"struct Later { int v; };\n"
	"NS_BEGIN\n"
	"template <int Count> struct Fixed { int Items[Count]; };\n"
	"int run(bool a, bool b)\n"
	"{\n"
	"\tTRY(a);\n"
	"\tVERIFY(a && !b);\n"
	"\treturn 0;\n"
	"}\n"
	"NS_END\n";

TEST_CASE("TranslationUnit enters macro namespaces and sees what macro arguments and calls contain")
{
	TempDir tmp;
	SyntaxTree tree = parse(tmp, g_Macros);
	std::vector<SyntaxNode const*> ss = named(tree);

	CHECK(count(ss, "lua_State") == 0);
	CHECK(count(ss, "Later") == 1);
	REQUIRE(find(ss, "run"));
	REQUIRE(find(ss, "Count"));
	CHECK(find(ss, "Count")->node.attributes.at("storage") == "template");
	CHECK(find(ss, "run")->node.attributes.at("scope") == "free");

	SyntaxNode const* body = find(ss, "run")->children.back();
	REQUIRE(body->node.type == "block");
	CHECK(types(body) == std::vector<std::string>{"statement", "statement", "statement", "return"});
	CHECK(body->children[0]->firstLine == 12);
	CHECK(body->children[1]->firstLine == 12);
	CHECK(body->children[2]->firstLine == 13);

	std::vector<std::string> spellings;

	for (SyntaxNode const* e : all(tree, "expression"))
	{
		spellings.push_back(e->operatorSpelling);
		CHECK(std::string(g_Macros).substr(e->operatorOffset, e->operatorSpelling.size()) == e->operatorSpelling);
	}

	CHECK(spellings == std::vector<std::string>{"!", "&&", "!"});
}
