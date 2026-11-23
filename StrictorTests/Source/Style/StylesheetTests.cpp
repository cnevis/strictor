#include "Style/Stylesheet.h"
#include "Support/TempDir.h"
#include "System/Text.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

static std::vector<Rule> parse(std::string const& text)
{
	TempDir tmp;
	tmp.write("rules.lua", "");

	return Stylesheet(tmp.write(".strictor", text)).rules();
}

static std::string error(std::string const& text)
{
	try
	{
		parse(text);
	}
	catch (std::exception const& e)
	{
		std::string msg = e.what();
		std::size_t at = msg.find(".strictor:");

		return at == std::string::npos ? msg : msg.substr(at + 10);
	}

	return "no error";
}

TEST_CASE("Stylesheet parses rules, selector lists, conditions and declarations")
{
	std::vector<Rule> rules = parse(
		"/* naming */\n"
		"type { name: PascalCase; }\n"
		"\n"
		"variable[storage=member][access=private], macro\n"
		"{\n"
		"\tname: m_PascalCase; /* inline */\n"
		"\tbanned-parts: Ui   Host\n"
		"}\n"
		"* { rename: index=idx raw=ptr; banned-names: root }\n");

	REQUIRE(rules.size() == 3);

	REQUIRE(rules[0].selectors.size() == 1);
	REQUIRE(rules[0].selectors[0].parts.size() == 1);
	CHECK(rules[0].selectors[0].combinators.empty());
	CHECK(rules[0].selectors[0].parts[0].type == "type");
	CHECK(rules[0].selectors[0].parts[0].conditions.empty());
	REQUIRE(rules[0].declarations.size() == 1);
	CHECK(rules[0].declarations[0].property == "name");
	CHECK(rules[0].declarations[0].values == std::vector<std::string>{"PascalCase"});

	REQUIRE(rules[1].selectors.size() == 2);
	Compound const& member = rules[1].selectors[0].parts[0];
	CHECK(member.type == "variable");
	REQUIRE(member.conditions.size() == 2);
	CHECK(member.conditions[0].attribute == "storage");
	CHECK(member.conditions[0].value == "member");
	CHECK(member.conditions[1].attribute == "access");
	CHECK(member.conditions[1].value == "private");
	CHECK(rules[1].selectors[1].parts[0].type == "macro");
	REQUIRE(rules[1].declarations.size() == 2);
	CHECK(rules[1].declarations[1].property == "banned-parts");
	CHECK(rules[1].declarations[1].values == std::vector<std::string>{"Ui", "Host"});

	CHECK(rules[2].selectors[0].parts[0].type == "*");
	REQUIRE(rules[2].declarations.size() == 2);
	CHECK(rules[2].declarations[0].values == std::vector<std::string>{"index=idx", "raw=ptr"});
	CHECK(rules[2].declarations[1].values == std::vector<std::string>{"root"});

	CHECK(parse("").empty());
	CHECK(parse("  /* only a comment */ \n").empty());
	CHECK(parse("type {}")[0].declarations.empty());
}

TEST_CASE("Stylesheet parses child and descendant combinators")
{
	std::vector<Rule> rules = parse(
		"control > condition { max-clauses: 3; }\n"
		"block   return, function block > control[keyword=if] {}\n"
		"control>condition>expression {}\n");

	REQUIRE(rules.size() == 3);

	Selector const& child = rules[0].selectors[0];
	REQUIRE(child.parts.size() == 2);
	CHECK(child.parts[0].type == "control");
	CHECK(child.parts[1].type == "condition");
	CHECK(child.combinators == std::vector<Combinator>{Combinator::CHILD});

	REQUIRE(rules[1].selectors.size() == 2);
	CHECK(rules[1].selectors[0].combinators == std::vector<Combinator>{Combinator::DESCENDANT});
	CHECK(rules[1].selectors[0].parts[1].type == "return");

	Selector const& mixed = rules[1].selectors[1];
	REQUIRE(mixed.parts.size() == 3);
	CHECK(mixed.combinators == std::vector<Combinator>{Combinator::DESCENDANT, Combinator::CHILD});
	CHECK(mixed.parts[2].conditions[0].value == "if");

	CHECK(rules[2].selectors[0].parts.size() == 3);
	CHECK(rules[2].selectors[0].combinators == std::vector<Combinator>{Combinator::CHILD, Combinator::CHILD});
}

TEST_CASE("Stylesheet accepts every selector, attribute value and property")
{
	std::string text;
	int expected = 0;

	for (char const* type : {"*", "type", "function", "variable", "enumerator", "macro", "namespace",
	                         "block", "statement", "control", "condition", "expression", "return", "case"})
	{
		text += std::string(type) + " { name: camelCase; }\n";
		expected++;
	}

	for (char const* storage : {"local", "parameter", "member", "global"})
	{
		text += std::string("variable[storage=") + storage + "] { banned-names: x; }\n";
		expected++;
	}

	for (char const* access : {"public", "protected", "private"})
	{
		text += std::string("function[access=") + access + "] { banned-parts: x; }\n";
		expected++;
	}

	for (char const* konst : {"constexpr", "const", "none"})
	{
		text += std::string("variable[const=") + konst + "] { banned-prefixes: x; banned-suffixes: y; rename: a=b; }\n";
		expected++;
	}

	for (char const* kw : {"if", "for", "while", "do", "switch", "try"})
	{
		text += std::string("control[keyword=") + kw + "][braced=yes], control[braced=no] { blank-before: 0; blank-after: 12; }\n";
		expected++;
	}

	text += "condition { max-clauses: 1; }\nexpression { logical-operators: words; }\n* { logical-operators: symbols; script: rules.lua ./rules.lua; }\n";
	expected += 3;

	text += "function[scope=free][linkage=internal][file=class][callers=none][types=plain] { helper: report; }\n"
	        "function[scope=member][linkage=external][file=other][callers=one-class][types=project] { helper: allow; }\n"
	        "function[callers=several] { helper-folders: System Math; helper-allow: floor clamp* ; }\n";
	expected += 3;

	text += "type[enum=plain] { report: use enum class; plural: report; plural-allow: Options; }\n"
	        "type[enum=class][storage=template] { plural: allow; }\n"
	        "variable { declarators: split; }\nvariable[storage=local] { declarators: keep; }\n"
	        "case { braces: insert; }\ncase[braced=no] { braces: keep; }\n";
	expected += 6;

	CHECK(parse(text).size() == (std::size_t)expected);
}

TEST_CASE("Stylesheet resolves scripts relative to its own folder")
{
	TempDir tmp;
	tmp.write("lua/check.lua", "");
	std::vector<Rule> rules = Stylesheet(tmp.write("sub/.strictor", "* { script: ../lua/check.lua; }")).rules();

	REQUIRE(rules.size() == 1);
	REQUIRE(rules[0].declarations[0].values.size() == 1);
	CHECK(std::filesystem::equivalent(utf8Path(rules[0].declarations[0].values[0]), tmp.path() / "lua" / "check.lua"));
}

TEST_CASE("Stylesheet reports errors with their line")
{
	struct Case
	{
		char const* text;
		char const* message;
	};

	Case const cases[] =
	{
		{"widget { name: camelCase; }", "1: unknown selector 'widget'"},
		{"variable[place=local] {}", "1: unknown attribute 'place'"},
		{"function[scope=local] {}", "1: unknown value 'local' for attribute 'scope'"},
		{"variable[storage=stack] {}", "1: unknown value 'stack' for attribute 'storage'"},
		{"control[keyword=goto] {}", "1: unknown value 'goto' for attribute 'keyword'"},
		{"control[braced=maybe] {}", "1: unknown value 'maybe' for attribute 'braced'"},
		{"variable[storage local] {}", "1: expected '='"},
		{"variable[storage=local {}", "1: expected ']'"},
		{"type {\n\tcolour: red;\n}", "2: unknown property 'colour'"},
		{"type {\n\n\tname: kebab-case;\n}", "3: unknown name style 'kebab-case'; use PascalCase, camelCase, UPPER_SNAKE or snake_case"},
		{"type { name: m-PascalCase; }", "1: invalid name prefix in 'm-PascalCase'"},
		{"type { name: camelCase kebab-case; }", "1: unknown name style 'kebab-case'; use PascalCase, camelCase, UPPER_SNAKE or snake_case"},
		{"function[name=] {}", "1: expected a name pattern"},
		{"function[name=a b] {}", "1: expected ']'"},
		{"type { rename: index; }", "1: rename takes from=to pairs"},
		{"type { rename: =idx; }", "1: rename takes from=to pairs"},
		{"type { rename: index=; }", "1: rename takes from=to pairs"},
		{"control { blank-before: x; }", "1: blank-before takes a number of lines"},
		{"control { blank-after: -1; }", "1: blank-after takes a number of lines"},
		{"control { blank-after: 1 2; }", "1: blank-after takes a number of lines"},
		{"control { blank-after: 1000; }", "1: blank-after takes a number of lines"},
		{"condition { max-clauses: 0; }", "1: max-clauses takes a number of at least 1"},
		{"expression { logical-operators: both; }", "1: logical-operators takes words or symbols"},
		{"* {\n\tscript: missing.lua;\n}", "2: script 'missing.lua' not found"},
		{"function { helper: maybe; }", "1: helper takes report or allow"},
		{"function { helper: report allow; }", "1: helper takes report or allow"},
		{"type { plural: maybe; }", "1: plural takes report or allow"},
		{"variable { declarators: one; }", "1: declarators takes split or keep"},
		{"case { braces: always; }", "1: braces takes insert or keep"},
		{"type[enum=scoped] {}", "1: unknown value 'scoped' for attribute 'enum'"},
		{"function[callers=two] {}", "1: unknown value 'two' for attribute 'callers'"},
		{"type { name: ; }", "1: property 'name' needs a value"},
		{"type { name camelCase; }", "1: expected ':'"},
		{"type { name: camelCase;", "1: expected '}'"},
		{"type { name: camelCase", "1: expected ';'"},
		{"type", "1: expected '{'"},
		{"type name: x", "1: unknown selector 'name'"},
		{"control > {}", "1: expected a name"},
		{"control > > condition {}", "1: expected a name"},
		{"type, {}", "1: expected a name"},
		{"type {}\n\n}", "3: expected a name"},
		{"/* open", "1: unterminated comment"},
		{"type {}\n/* a\nb\nc", "2: unterminated comment"}
	};

	for (Case const& c : cases)
	{
		INFO(c.text);
		CHECK(error(c.text) == c.message);
	}
}

TEST_CASE("Stylesheet fails for a missing file")
{
	TempDir tmp;
	CHECK_THROWS_AS(Stylesheet(tmp.path() / ".strictor"), std::runtime_error);
}

TEST_CASE("Selector matches node types and attributes and ranks specificity")
{
	Node local = {"variable", {{"storage", "local"}, {"const", "none"}}};
	Node member = {"variable", {{"storage", "member"}, {"access", "private"}, {"const", "const"}}};
	Node fn = {"function", {}};

	Selector any = {{{"*", {}}}, {}};
	Selector variable = {{{"variable", {}}}, {}};
	Selector localVar = {{{"variable", {{"storage", "local"}}}}, {}};
	Selector privateAny = {{{"*", {{"access", "private"}}}}, {}};
	Selector privateConstMember = {{{"variable", {{"storage", "member"}, {"access", "private"}, {"const", "const"}}}}, {}};

	CHECK(any.matches(local));
	CHECK(any.matches(fn));
	CHECK(variable.matches(local));
	CHECK(variable.matches(member));
	CHECK_FALSE(variable.matches(fn));
	CHECK(localVar.matches(local));
	CHECK_FALSE(localVar.matches(member));
	CHECK(privateAny.matches(member));
	CHECK_FALSE(privateAny.matches(local));
	CHECK_FALSE(privateAny.matches(fn));
	CHECK(privateConstMember.matches(member));

	CHECK(any.specificity() == 0);
	CHECK(variable.specificity() == 1);
	CHECK(privateAny.specificity() == 10);
	CHECK(localVar.specificity() == 11);
	CHECK(privateConstMember.specificity() == 31);
}

TEST_CASE("Selector follows child and descendant combinators up the parent chain")
{
	Node function = {"function", {}};
	Node body = {"block", {}, &function};
	Node control = {"control", {{"keyword", "if"}}, &body};
	Node condition = {"condition", {}, &control};
	Node expression = {"expression", {}, &condition};
	Node loose = {"condition", {}};

	Selector child = {{{"control", {}}, {"condition", {}}}, {Combinator::CHILD}};
	Selector descendant = {{{"function", {}}, {"expression", {}}}, {Combinator::DESCENDANT}};
	Selector chain = {{{"block", {}}, {"control", {{"keyword", "if"}}}, {"condition", {}}}, {Combinator::CHILD, Combinator::CHILD}};
	Selector wrongKeyword = {{{"control", {{"keyword", "for"}}}, {"condition", {}}}, {Combinator::CHILD}};
	Selector skipLevel = {{{"block", {}}, {"condition", {}}}, {Combinator::CHILD}};

	CHECK(child.matches(condition));
	CHECK_FALSE(child.matches(loose));
	CHECK_FALSE(child.matches(expression));
	CHECK(descendant.matches(expression));
	CHECK_FALSE(descendant.matches(function));
	CHECK(chain.matches(condition));
	CHECK_FALSE(wrongKeyword.matches(condition));
	CHECK_FALSE(skipLevel.matches(condition));

	CHECK(child.specificity() == 2);
	CHECK(chain.specificity() == 13);
}

TEST_CASE("Selector matches names by exact name or * pattern")
{
	std::vector<Rule> rules = parse(
		"namespace[name=impl] function[scope=free] { helper: allow; }\n"
		"function[name=is*], function[name=*To], variable[name=m_*Count] {}\n"
		"variable[storage=template] { name: PascalCase UPPER_SNAKE; }\n");

	REQUIRE(rules.size() == 3);
	CHECK(rules[0].selectors[0].parts[0].conditions[0].attribute == "name");
	CHECK(rules[0].selectors[0].parts[0].conditions[0].value == "impl");
	CHECK(rules[0].selectors[0].specificity() == 22);

	Node impl = {"namespace", {}, nullptr, "impl"};
	Node other = {"namespace", {}, nullptr, "gui"};
	Node helper = {"function", {{"scope", "free"}}, &impl, "twice"};
	Node loose = {"function", {{"scope", "free"}}, &other, "twice"};
	Node predicate = {"function", {}, nullptr, "isOpen"};
	Node unnamed = {"function", {}, nullptr, ""};
	Node counter = {"variable", {}, nullptr, "m_ItemCount"};

	CHECK(rules[0].selectors[0].matches(helper));
	CHECK_FALSE(rules[0].selectors[0].matches(loose));
	CHECK(rules[1].selectors[0].matches(predicate));
	CHECK_FALSE(rules[1].selectors[0].matches(unnamed));
	CHECK_FALSE(rules[1].selectors[1].matches(predicate));
	CHECK(rules[1].selectors[2].matches(counter));
}
