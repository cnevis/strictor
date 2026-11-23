#include "Rules/Layout.h"
#include "Rules/SourceText.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

static std::string space(std::string const& text, std::vector<Spacing> const& statements)
{
	SourceText src(text);
	std::vector<Edit> edits;
	spaceStatements(src, statements, &edits);

	return src.apply(edits);
}

static std::string convert(std::string const& text, std::string const& spelling, std::string const& mode)
{
	SourceText src(text);
	std::vector<Edit> edits;
	convertOperator(src, text.find(spelling), spelling, mode, &edits);

	return src.apply(edits);
}

static std::string spaceAfterNot(std::string const& text, std::string const& spelling)
{
	SourceText src(text);
	std::vector<Edit> edits;
	spaceNot(src, text.find(spelling), spelling, &edits);

	return src.apply(edits);
}

TEST_CASE("SourceText knows lines, blank lines and line endings")
{
	SourceText src("a\n\n  \t\nb\nc");

	CHECK(src.lineCount() == 5);
	CHECK(src.lineStart(1) == 0);
	CHECK(src.lineStart(2) == 2);
	CHECK(src.lineStart(4) == 7);
	CHECK(src.lineStart(3) == 3);
	CHECK(src.lineStart(6) == 10);
	CHECK(src.lineStart(0) == 0);
	CHECK_FALSE(src.blank(1));
	CHECK(src.blank(2));
	CHECK(src.blank(3));
	CHECK_FALSE(src.blank(4));
	CHECK_FALSE(src.blank(9));
	CHECK(src.blankLinesAbove(4) == 2);
	CHECK(src.blankLinesAbove(2) == 0);
	CHECK(src.blankLinesAbove(1) == 0);
	CHECK(src.eol() == "\n");
	CHECK(src.content() == "a\n\n  \t\nb\nc");

	SourceText crlf("a\r\n\r\nb\r\n");

	CHECK(crlf.eol() == "\r\n");
	CHECK(crlf.blank(2));
	CHECK_FALSE(crlf.blank(4));
	CHECK(crlf.lineCount() == 4);
}

TEST_CASE("SourceText applies edits from the end, skipping overlaps")
{
	SourceText src("0123456789");

	CHECK(src.apply({}) == "0123456789");
	CHECK(src.apply({{2, 3, "ab"}, {8, 0, "X"}, {0, 1, ""}}) == "1ab567X89");
	CHECK(src.apply({{2, 4, "ab"}, {4, 2, "Z"}}) == "0123Z6789");
	CHECK(src.apply({{20, 1, "out of range"}}) == "0123456789");
}

TEST_CASE("spaceStatements sets the blank lines between sibling statements")
{
	std::string text = "a();\nif (x) {}\nb();\n";

	CHECK(space(text, {{1, 1}, {2, 2, 1, 1}, {3, 3}}) == "a();\n\nif (x) {}\n\nb();\n");
	CHECK(space(text, {{1, 1}, {2, 2, 1, -1}, {3, 3}}) == "a();\n\nif (x) {}\nb();\n");
	CHECK(space(text, {{1, 1}, {2, 2}, {3, 3}}) == text);

	std::string loose = "a();\n\n\n\nif (x) {}\n\n\nb();\n";

	CHECK(space(loose, {{1, 1}, {5, 5, 1, 1}, {8, 8}}) == "a();\n\nif (x) {}\n\nb();\n");
	CHECK(space(loose, {{1, 1}, {5, 5, 0, 0}, {8, 8}}) == "a();\nif (x) {}\nb();\n");
	CHECK(space(loose, {{1, 1, -1, 2}, {5, 5}, {8, 8, 2, -1}}) == "a();\n\n\nif (x) {}\n\n\nb();\n");
}

TEST_CASE("spaceStatements takes the larger of after and before, keeps comments attached and ignores shared lines")
{
	CHECK(space("if (a) {}\nif (b) {}\n", {{1, 1, -1, 0}, {2, 2, 1, -1}}) == "if (a) {}\n\nif (b) {}\n");
	CHECK(space("a();\n// why\nif (x) {}\n", {{1, 1}, {3, 3, 1, -1}}) == "a();\n\n// why\nif (x) {}\n");
	CHECK(space("a();\n\n// why\n\nif (x) {}\n", {{1, 1}, {5, 5, 1, -1}}) == "a();\n\n// why\n\nif (x) {}\n");
	CHECK(space("a(); if (x) {}\n", {{1, 1}, {1, 1, 1, 1}}) == "a(); if (x) {}\n");
	CHECK(space("if (x)\n{\n}\nb();\n", {{1, 3, -1, 1}, {4, 4}}) == "if (x)\n{\n}\n\nb();\n");
	CHECK(space("a();\r\nif (x) {}\r\n", {{1, 1}, {2, 2, 1, -1}}) == "a();\r\n\r\nif (x) {}\r\n");
	CHECK(space("a();\n", {{1, 1, 1, 1}}) == "a();\n");
}

TEST_CASE("convertOperator switches between symbols and words, and spaceNot keeps not apart from its operand")
{
	CHECK(convert("a && b", "&&", "words") == "a and b");
	CHECK(convert("a || b", "||", "words") == "a or b");
	CHECK(convert("!b", "!", "words") == "not b");
	CHECK(convert("!(a)", "!", "words") == "not (a)");
	CHECK(convert("! b", "!", "words") == "not b");
	CHECK(convert("a and b", "and", "words") == "a and b");

	CHECK(convert("a and b", "and", "symbols") == "a && b");
	CHECK(convert("a or b", "or", "symbols") == "a || b");
	CHECK(convert("not b", "not", "symbols") == "!b");
	CHECK(convert("not  \tb", "not", "symbols") == "!b");
	CHECK(convert("a && b", "&&", "symbols") == "a && b");

	CHECK(spaceAfterNot("not(a)", "not") == "not (a)");
	CHECK(spaceAfterNot("not (a)", "not") == "not (a)");
	CHECK(spaceAfterNot("not\n(a)", "not") == "not\n(a)");
	CHECK(spaceAfterNot("!(a)", "!") == "!(a)");
}

static std::string split(std::string const& text, std::vector<std::string> const& names, std::vector<Diagnostic>* diagnostics)
{
	SourceText src(text);
	std::vector<Declarator> group;
	std::vector<Edit> edits;
	std::size_t start = text.find_first_not_of(" \t");
	std::size_t from = start;

	for (std::string const& name : names)
	{
		std::size_t at = text.find(name, from);
		std::size_t end = text.find_first_of(",;", at);
		std::size_t brace = text.find('{', at);

		if (brace < end)
		{
			end = text.find('}', brace) + 1;
		}

		group.push_back({start, at, end, 1, (int)start + 1});
		from = end;
	}

	splitDeclarators(src, group, &edits, diagnostics);

	return src.apply(edits);
}

static std::string brace(std::string const& text, std::vector<CaseBody> const& bodies)
{
	SourceText src(text);
	std::vector<Edit> edits;
	braceCases(src, bodies, &edits);

	return src.apply(edits);
}

TEST_CASE("splitDeclarators gives every declarator its own declaration")
{
	std::vector<Diagnostic> ds;

	CHECK(split("\tfloat x, y;\n", {"x", "y"}, &ds) == "\tfloat x;\n\tfloat y;\n");
	CHECK(split("int const *a = &v, b{2}, *c;\n", {"a", "b", "c"}, &ds) == "int const *a = &v;\nint const b{2};\nint const *c;\n");
	CHECK(split("static std::map<int, int> mm, nn;\r\n", {"mm", "nn"}, &ds) == "static std::map<int, int> mm;\r\nstatic std::map<int, int> nn;\r\n");
	CHECK(ds.empty());

	CHECK(split("int (*f)(int), g;\n", {"f", "g"}, &ds) == "int (*f)(int), g;\n");
	REQUIRE(ds.size() == 1);
	CHECK(ds[0].rule == "layout.declarators");
}

TEST_CASE("braceCases wraps unbraced case bodies and keeps braced ones")
{
	std::string text = "case 0: a = 1; break;\ncase 1: { b(); break; }\ndefault: c();\n";
	std::size_t a = text.find("a = 1");
	std::size_t b = text.find("{ b()");
	std::size_t c = text.find("c()");

	std::vector<CaseBody> bodies =
	{
		{a, text.find("break;") + 5, false},
		{b, text.find("}") + 1, true},
		{c, c + 3, false},
	};

	CHECK(brace(text, bodies) == "case 0: {\na = 1; break;\n}\ncase 1: { b(); break; }\ndefault: {\nc();\n}\n");
}

TEST_CASE("checkClauses reports conditions over the limit")
{
	std::vector<Diagnostic> ds;
	checkClauses(4, 9, 3, 3, &ds);

	CHECK(ds.empty());

	checkClauses(4, 9, 5, 3, &ds);

	REQUIRE(ds.size() == 1);
	CHECK(ds[0].line == 4);
	CHECK(ds[0].column == 9);
	CHECK(ds[0].rule == "layout.condition");
	CHECK(ds[0].message == "condition has 5 clauses; split into guard clauses (max 3)");
}
