#include "System/Text.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Text converts between UTF-8, paths and wide strings")
{
	std::string s = "Käyttäjä/日本/€.cpp";
	std::filesystem::path p = utf8Path(s);

	CHECK(p.wstring() == L"Käyttäjä/日本/€.cpp");
	CHECK(utf8(p) == s);
	CHECK(wide(s) == L"Käyttäjä/日本/€.cpp");
	CHECK(wide("").empty());
	CHECK(utf8(std::filesystem::path()).empty());
}

TEST_CASE("Text trims whitespace and changes ASCII case only")
{
	CHECK(trim("  a b \r\n") == "a b");
	CHECK(trim("\t\r\n ") == "");
	CHECK(trim("") == "");
	CHECK(trim("x") == "x");

	CHECK(lowercase("HeLLo_World9 ÄÖ") == "hello_world9 ÄÖ");
	CHECK(uppercase("heLLo_world9 äö") == "HELLO_WORLD9 äö");

	CHECK(upperLetter('A'));
	CHECK(upperLetter('Z'));
	CHECK_FALSE(upperLetter('a'));
	CHECK_FALSE(upperLetter('_'));
	CHECK(lowerLetter('a'));
	CHECK(lowerLetter('z'));
	CHECK_FALSE(lowerLetter('A'));
	CHECK_FALSE(lowerLetter('\xC3'));
	CHECK(digit('0'));
	CHECK(digit('9'));
	CHECK_FALSE(digit('a'));
}
