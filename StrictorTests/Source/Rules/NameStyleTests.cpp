#include "Rules/NameStyle.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

static void check(NameStyle const& style, std::vector<std::string> const& good, std::vector<std::string> const& bad)
{
	for (std::string const& name : good)
	{
		INFO(name);
		CHECK(style.matches(name));
	}

	for (std::string const& name : bad)
	{
		INFO(name);
		CHECK_FALSE(style.matches(name));
	}
}

TEST_CASE("NameStyle matches each letter case")
{
	check(NameStyle("PascalCase"),
	      {"Widget", "W", "UIFont", "Vec3", "HTTPRequest"},
	      {"", "widget", "Wid_get", "Widget_", "_Widget", "3D", "Wid-get", "Wäger"});

	check(NameStyle("camelCase"),
	      {"decodeFrame", "x", "a1", "parseHTTP"},
	      {"", "DecodeFrame", "decode_frame", "decodeFrame_", "_decode", "1x"});

	check(NameStyle("UPPER_SNAKE"),
	      {"MAX_SIZE", "X", "A1_B2", "SNAKE_GAME_WIDTH"},
	      {"", "Max_Size", "MAX__SIZE", "MAX_", "_MAX", "1MAX", "MAXsize"});

	check(NameStyle("snake_case"),
	      {"decode_frame", "x2", "a_b_c"},
	      {"", "Decode_frame", "decode__frame", "decode_", "_decode", "decodeFrame"});
}

TEST_CASE("NameStyle requires and exposes its prefix")
{
	NameStyle member("m_PascalCase");

	CHECK(member.prefix() == "m_");
	check(member, {"m_Count", "m_X"}, {"m_count", "Count", "m_", "m__Count", "M_Count"});

	NameStyle global("g_PascalCase");

	CHECK(global.prefix() == "g_");
	check(global, {"g_Joystick"}, {"Joystick", "g_joystick"});

	NameStyle konst("kPascalCase");

	CHECK(konst.prefix() == "k");
	check(konst, {"kMaxValue"}, {"MaxValue", "kmaxValue"});

	CHECK(NameStyle("camelCase").prefix().empty());
	CHECK(NameStyle("s_UPPER_SNAKE").matches("s_LIMIT"));
}

TEST_CASE("NameStyle rejects unknown styles and invalid prefixes")
{
	CHECK_THROWS(NameStyle(""));
	CHECK_THROWS(NameStyle("kebab-case"));
	CHECK_THROWS(NameStyle("pascalcase"));
	CHECK_THROWS(NameStyle("m-PascalCase"));
	CHECK_THROWS(NameStyle("m PascalCase"));
}
