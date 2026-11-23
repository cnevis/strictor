#include "Rules/Design.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

static std::vector<std::string> check(FreeFunction const& f, Properties const& props)
{
	std::vector<Diagnostic> ds;
	checkHelper(f, props, &ds);

	std::vector<std::string> lines;

	for (Diagnostic const& d : ds)
	{
		lines.push_back(std::to_string(d.line) + ":" + std::to_string(d.column) + " " + d.rule + ": " + d.message);
	}

	return lines;
}

static FreeFunction helper(std::string const& name)
{
	FreeFunction f;
	f.name = name;
	f.line = 7;
	f.column = 13;
	f.owner = "NameStyle";
	f.staticInClassFile = true;
	f.oneClass = true;
	f.path = "C:/Project/Source/Rules/NameStyle.cpp";

	return f;
}

using Lines = std::vector<std::string>;

TEST_CASE("checkHelper reports only when the cascade says report")
{
	FreeFunction f = helper("upper");

	CHECK(check(f, {{"helper", {"report"}}}) == Lines{
		"7:13 design.helper: free function 'upper' belongs in class 'NameStyle' (static in its .cpp, called only by NameStyle)"});
	CHECK(check(f, {{"helper", {"allow"}}}).empty());
	CHECK(check(f, {}).empty());
}

TEST_CASE("checkHelper names the reasons it knows and falls back without an owner")
{
	Properties report = {{"helper", {"report"}}};
	FreeFunction f = helper("upper");

	f.oneClass = false;
	CHECK(check(f, report) == Lines{"7:13 design.helper: free function 'upper' belongs in class 'NameStyle' (static in its .cpp)"});

	f.staticInClassFile = false;
	f.oneClass = true;
	CHECK(check(f, report) == Lines{"7:13 design.helper: free function 'upper' belongs in class 'NameStyle' (called only by NameStyle)"});

	f.oneClass = false;
	CHECK(check(f, report) == Lines{"7:13 design.helper: free function 'upper' belongs in class 'NameStyle'"});

	f.owner.clear();
	CHECK(check(f, report) == Lines{"7:13 design.helper: free function 'upper' should be a member function"});
}

TEST_CASE("checkHelper excuses allowed names and utility folders")
{
	Properties props =
	{
		{"helper", {"report"}},
		{"helper-allow", {"floor", "clamp*", "*Of", "a*b*c"}},
		{"helper-folders", {"system", "Math"}}
	};

	for (char const* name : {"floor", "clamp", "clampValue", "sizeOf", "aXbYc", "abc"})
	{
		INFO(name);
		CHECK(check(helper(name), props).empty());
	}

	for (char const* name : {"floored", "unclamp", "Of_", "acb"})
	{
		INFO(name);
		CHECK(check(helper(name), props).size() == 1);
	}

	FreeFunction math = helper("upper");
	math.path = "C:/Project/Source/math/detail/Util.cpp";

	CHECK(check(math, props).empty());

	FreeFunction system = helper("upper");
	system.path = "C:/Project/System/Util.cpp";

	CHECK(check(system, props).empty());

	FreeFunction named = helper("upper");
	named.path = "C:/Project/Source/Mathematics/Util.cpp";

	CHECK(check(named, props).size() == 1);

	FreeFunction file = helper("upper");
	file.path = "C:/Project/Source/Math.cpp";

	CHECK(check(file, props).size() == 1);
}
