#include "Format/LineDelta.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>
#include <vector>

using Ranges = std::vector<std::pair<int, int>>;

TEST_CASE("LineDelta marks the lines of the text that differ from the base")
{
	CHECK(LineDelta("a\nb\nc\n", "a\nb\nc\n").empty());
	CHECK(LineDelta("a\r\nb\r\n", "a\nb\n").empty());
	CHECK(LineDelta("a\nb\nc\n", "a\nc\n").empty());
	CHECK(LineDelta("a\nb\nc\n", "a\nx\ny\nb\nc\n").ranges() == Ranges{{2, 3}});
	CHECK(LineDelta("a\nb\nc\nd\n", "a\nB\nc\nD\n").ranges() == Ranges{{2, 2}, {4, 4}});
	CHECK(LineDelta("", "a\nb\n").ranges() == Ranges{{1, 2}});
	CHECK(LineDelta("a\nb", "a\nb\nc").ranges() == Ranges{{3, 3}});

	LineDelta d("x\na\nx\nb\nx\n", "x\na\nnew\nx\nb\nx\n");

	CHECK(d.contains(3));
	CHECK_FALSE(d.contains(2));
	CHECK_FALSE(d.contains(4));
	CHECK_FALSE(d.contains(0));
	CHECK_FALSE(d.contains(99));
}

TEST_CASE("LineDelta keeps only edits and diagnostics that touch changed lines")
{
	std::string base = "one && two;\nkeep && this;\n";
	std::string text = "one && two;\nnew && line;\nkeep && this;\n";
	LineDelta d(base, text);
	SourceText src(text);

	std::vector<Edit> edits =
	{
		{text.find("&&"), 2, "and"},
		{text.find("new &&") + 4, 2, "and"},
		{text.find("keep &&") + 5, 2, "and"},
		{src.lineStart(3), 0, "\n"},
		{src.lineStart(2), 0, "\n"},
	};

	d.filter(src, &edits);

	REQUIRE(edits.size() == 3);
	CHECK(edits[0].offset == text.find("new &&") + 4);
	CHECK(edits[1].offset == src.lineStart(3));
	CHECK(edits[2].offset == src.lineStart(2));

	std::vector<Diagnostic> ds = {{1, 1, "a", "x"}, {2, 1, "b", "y"}, {3, 1, "c", "z"}};
	d.filter(&ds);

	REQUIRE(ds.size() == 1);
	CHECK(ds[0].line == 2);
}
