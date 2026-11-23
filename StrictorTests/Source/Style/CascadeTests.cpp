#include "Style/Cascade.h"
#include "Support/TempDir.h"

#include <catch2/catch_test_macros.hpp>

#include <string>

static Node const g_Local = {"variable", {{"storage", "local"}, {"const", "none"}}};
static Node const g_Parameter = {"variable", {{"storage", "parameter"}, {"const", "none"}}};
static Node const g_PrivateMember = {"variable", {{"storage", "member"}, {"access", "private"}, {"const", "none"}}};
static Node const g_Function = {"function", {}};

static Cascade cascade(std::vector<std::string> const& sheets)
{
	TempDir tmp;
	Cascade c;
	int i = 0;

	for (std::string const& text : sheets)
	{
		c.append(Stylesheet(tmp.write(std::to_string(i) + "/.strictor", text)));
		i++;
	}

	return c;
}

TEST_CASE("Cascade lets the more specific selector win regardless of order")
{
	Cascade c = cascade({
		"variable[storage=local] { name: camelCase; }\n"
		"variable { name: snake_case; }\n"
		"* { name: PascalCase; }\n"});

	CHECK(c.compute(g_Local).at("name") == std::vector<std::string>{"camelCase"});
	CHECK(c.compute(g_Parameter).at("name") == std::vector<std::string>{"snake_case"});
	CHECK(c.compute(g_Function).at("name") == std::vector<std::string>{"PascalCase"});
}

TEST_CASE("Cascade lets the later rule win at equal specificity and merges properties")
{
	Cascade c = cascade({
		"variable { name: camelCase; banned-parts: Ui; }\n"
		"variable { name: snake_case; }\n"
		"function, variable[storage=member] { rename: index=idx; }\n"});

	Properties local = c.compute(g_Local);

	CHECK(local.at("name") == std::vector<std::string>{"snake_case"});
	CHECK(local.at("banned-parts") == std::vector<std::string>{"Ui"});
	CHECK(local.count("rename") == 0);

	CHECK(c.compute(g_PrivateMember).at("rename") == std::vector<std::string>{"index=idx"});
	CHECK(c.compute(g_Function).at("rename") == std::vector<std::string>{"index=idx"});
	CHECK(c.compute(g_Function).count("name") == 0);
}

TEST_CASE("Cascade uses the most specific matching selector in a selector list")
{
	Cascade c = cascade({
		"variable[storage=member][access=private], function { name: m_PascalCase; }\n"
		"variable[storage=member] { name: PascalCase; }\n"});

	CHECK(c.compute(g_PrivateMember).at("name") == std::vector<std::string>{"m_PascalCase"});
}

TEST_CASE("Cascade lets nearer sheets win ties but not beat more specific outer rules")
{
	Cascade c = cascade({
		"variable { name: camelCase; }\n"
		"variable[storage=member][access=private] { name: m_PascalCase; }\n",
		"variable { name: snake_case; }\n"});

	CHECK(c.compute(g_Local).at("name") == std::vector<std::string>{"snake_case"});
	CHECK(c.compute(g_PrivateMember).at("name") == std::vector<std::string>{"m_PascalCase"});
}

TEST_CASE("Cascade computes nothing when no rule matches")
{
	CHECK(cascade({"function { name: camelCase; }"}).compute(g_Local).empty());
	CHECK(cascade({}).compute(g_Function).empty());
}
