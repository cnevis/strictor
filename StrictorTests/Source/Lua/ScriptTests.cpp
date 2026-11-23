#include "Clang/TranslationUnit.h"
#include "Lua/Script.h"
#include "Support/Fixtures.h"
#include "Support/TempDir.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <string>
#include <vector>

using Catch::Matchers::ContainsSubstring;

struct ScriptRun
{
	std::vector<Diagnostic> diagnostics;
	std::vector<Edit> edits;
	std::string result;
};

static ScriptRun run(TempDir const& tmp, std::string const& lua, std::string const& code)
{
	toolchain();
	Script script(tmp.write("rules.lua", lua));
	TranslationUnit tu(tmp.path() / "a.cpp", code, {tmp.path(), {"-xc++", "-std=c++2c"}});
	SyntaxTree tree = tu.tree();
	SourceText src(code);
	ScriptRun r;
	RuleContext ctx(src, &r.diagnostics, &r.edits);

	for (std::unique_ptr<SyntaxNode> const& n : tree.nodes())
	{
		script.run(*n, &ctx);
	}

	r.result = src.apply(r.edits);

	return r;
}

static char const g_Code[] =
	"int twice(int v)\n"
	"{\n"
	"\treturn v * 2;\n"
	"}\n"
	"\n"
	"int pick(int v)\n"
	"{\n"
	"\tswitch (v)\n"
	"\t{\n"
	"\t\tcase 1:\n"
	"\t\t\treturn 3;\n"
	"\t}\n"
	"\n"
	"\treturn 0;\n"
	"}\n";

TEST_CASE("Script calls handlers for each node of their type with node details")
{
	TempDir tmp;
	ScriptRun r = run(tmp,
		"on('function', function(node, r)\n"
		"\tr:report('test.function', node, node.name .. ' ' .. node:lines() .. ' ' .. node.line .. ':' .. node.column)\n"
		"end)\n"
		"on('return', function(node, r)\n"
		"\tr:report('test.return', node, node.parent.type .. ' ' .. node:blankBefore())\n"
		"end)\n"
		"on('variable', function(node, r)\n"
		"\tr:report('test.variable', node, node.attributes.storage .. ' ' .. node.attributes.const)\n"
		"end)\n",
		g_Code);

	std::vector<std::string> lines;

	for (Diagnostic const& d : r.diagnostics)
	{
		lines.push_back(std::to_string(d.line) + ":" + std::to_string(d.column) + " " + d.rule + " " + d.message);
	}

	CHECK(lines == std::vector<std::string>{
		"1:5 test.function twice 4 1:5",
		"1:15 test.variable parameter none",
		"3:2 test.return block 0",
		"6:5 test.function pick 10 6:5",
		"6:14 test.variable parameter none",
		"11:4 test.return case 0",
		"14:2 test.return block 1"});
	CHECK(r.edits.empty());
}

TEST_CASE("Script inserts text before nodes")
{
	TempDir tmp;
	ScriptRun r = run(tmp,
		"on('return', function(node, r)\n"
		"\tif node.parent.type == 'case' and node:blankBefore() == 0 then\n"
		"\t\tr:insertBefore(node, '\\n')\n"
		"\tend\n"
		"end)\n",
		g_Code);

	REQUIRE(r.edits.size() == 1);
	CHECK_THAT(r.result, ContainsSubstring("\t\tcase 1:\n\t\t\t\nreturn 3;"));
}

TEST_CASE("Script supports several handlers per type and nil parents")
{
	TempDir tmp;
	ScriptRun r = run(tmp,
		"on('function', function(node, r) r:report('a', node, tostring(node.parent == nil)) end)\n"
		"on('function', function(node, r) r:report('b', node, 'second') end)\n",
		"int f() { return 0; }\n");

	REQUIRE(r.diagnostics.size() == 2);
	CHECK(r.diagnostics[0].message == "true");
	CHECK(r.diagnostics[1].message == "second");
}

TEST_CASE("Script reports load and runtime errors with the script location")
{
	TempDir tmp;
	toolchain();

	CHECK_THROWS_WITH(Script(tmp.write("syntax.lua", "on('return', function(\n")), ContainsSubstring("syntax.lua:"));
	CHECK_THROWS_WITH(Script(tmp.write("type.lua", "on('statment', function() end)\n")), ContainsSubstring("unknown node type 'statment'"));
	CHECK_THROWS_WITH(Script(tmp.write("top.lua", "error('boom')\n")), ContainsSubstring("top.lua:1: boom"));
	CHECK_THROWS_AS(Script(tmp.path() / "missing.lua"), std::runtime_error);

	CHECK_THROWS_WITH(run(tmp, "on('return', function(node, r) local x = nil; x.y = 1 end)\n", g_Code),
	                  ContainsSubstring("rules.lua:1:"));
}
