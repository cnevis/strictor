#include "Rules/Naming.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

static std::vector<std::string> check(std::string const& label, std::string const& name, Properties const& props)
{
	std::vector<Diagnostic> ds;
	Identifier id = {label, name, 1, 1};
	checkNaming(id, props, &ds);

	std::vector<std::string> lines;

	for (Diagnostic const& d : ds)
	{
		lines.push_back(d.rule + ": " + d.message);
	}

	return lines;
}

using Lines = std::vector<std::string>;

TEST_CASE("checkNaming reports names that break the name style")
{
	Properties props = {{"name", {"camelCase"}}};

	CHECK(check("local", "value", props).empty());
	CHECK(check("local", "Value", props) == Lines{"naming.case: local 'Value' should be camelCase"});
	CHECK(check("function", "do_thing", props) == Lines{"naming.case: function 'do_thing' should be camelCase"});

	std::vector<Diagnostic> ds;
	Identifier id = {"member", "count", 12, 9};
	checkNaming(id, {{"name", {"m_PascalCase"}}}, &ds);

	REQUIRE(ds.size() == 1);
	CHECK(ds[0].line == 12);
	CHECK(ds[0].column == 9);
	CHECK(ds[0].rule == "naming.case");
	CHECK(ds[0].message == "member 'count' should be m_PascalCase");
}

TEST_CASE("checkNaming matches banned names exactly and banned parts by word")
{
	Properties names = {{"banned-names", {"root", "smoke"}}};

	CHECK(check("local", "root", names) == Lines{"naming.banned: local 'root' is a banned name"});
	CHECK(check("local", "Root", names).empty());
	CHECK(check("local", "rootDir", names).empty());

	Properties parts = {{"banned-parts", {"Ui", "Host"}}};

	CHECK(check("type", "UiState", parts) == Lines{"naming.banned: type 'UiState' contains banned part 'Ui'"});
	CHECK(check("type", "UIFontFamily", parts) == Lines{"naming.banned: type 'UIFontFamily' contains banned part 'Ui'"});
	CHECK(check("local", "ui", parts) == Lines{"naming.banned: local 'ui' contains banned part 'Ui'"});
	CHECK(check("macro", "LIB_UI_SCALE", parts) == Lines{"naming.banned: macro 'LIB_UI_SCALE' contains banned part 'Ui'"});
	CHECK(check("type", "PluginHost", parts) == Lines{"naming.banned: type 'PluginHost' contains banned part 'Host'"});
	CHECK(check("type", "Quiet", parts).empty());
	CHECK(check("type", "Guide", parts).empty());
	CHECK(check("local", "ghostly", parts).empty());

	CHECK(check("type", "UiHost", parts) == Lines{
		"naming.banned: type 'UiHost' contains banned part 'Ui'",
		"naming.banned: type 'UiHost' contains banned part 'Host'"});
}

TEST_CASE("checkNaming checks the first and last word against banned prefixes and suffixes")
{
	Properties props =
	{
		{"banned-prefixes", {"out", "with"}},
		{"banned-suffixes", {"Of", "For", "At"}}
	};

	CHECK(check("function", "parentOf", props) == Lines{"naming.suffix: function 'parentOf' ends with banned suffix 'Of'"});
	CHECK(check("function", "routeFor", props) == Lines{"naming.suffix: function 'routeFor' ends with banned suffix 'For'"});
	CHECK(check("function", "dayOfWeek", props).empty());
	CHECK(check("function", "forEach", props).empty());
	CHECK(check("function", "format", props).empty());
	CHECK(check("parameter", "outValue", props) == Lines{"naming.prefix: parameter 'outValue' starts with banned prefix 'out'"});
	CHECK(check("parameter", "out", props) == Lines{
		"naming.prefix: parameter 'out' starts with banned prefix 'out'"});
	CHECK(check("parameter", "output", props).empty());
	CHECK(check("function", "withSeconds", props) == Lines{"naming.prefix: function 'withSeconds' starts with banned prefix 'with'"});
	CHECK(check("macro", "OUT_DIR", props) == Lines{"naming.prefix: macro 'OUT_DIR' starts with banned prefix 'out'"});
}

TEST_CASE("checkNaming ignores the style prefix when it splits words")
{
	Properties props =
	{
		{"name", {"m_PascalCase"}},
		{"banned-prefixes", {"out", "m"}},
		{"rename", {"index=idx"}}
	};

	CHECK(check("member", "m_OutValue", props) == Lines{"naming.prefix: member 'm_OutValue' starts with banned prefix 'out'"});
	CHECK(check("member", "m_Index", props) == Lines{"naming.rename: member 'm_Index' should be 'm_Idx'"});
	CHECK(check("member", "outValue", props) == Lines{
		"naming.case: member 'outValue' should be m_PascalCase",
		"naming.prefix: member 'outValue' starts with banned prefix 'out'"});
}

TEST_CASE("checkNaming suggests renames for whole names and words, keeping the word's case")
{
	Properties props = {{"rename", {"index=idx", "raw=ptr", "colour=color", "toString=string"}}};

	CHECK(check("local", "index", props) == Lines{"naming.rename: local 'index' should be 'idx'"});
	CHECK(check("local", "Index", props) == Lines{"naming.rename: local 'Index' should be 'Idx'"});
	CHECK(check("macro", "INDEX_COUNT", props) == Lines{"naming.rename: macro 'INDEX_COUNT' should be 'IDX_COUNT'"});
	CHECK(check("local", "colourIndex", props) == Lines{"naming.rename: local 'colourIndex' should be 'colorIdx'"});
	CHECK(check("local", "maxIndex", props) == Lines{"naming.rename: local 'maxIndex' should be 'maxIdx'"});
	CHECK(check("function", "toString", props) == Lines{"naming.rename: function 'toString' should be 'string'"});
	CHECK(check("type", "ToString", props) == Lines{"naming.rename: type 'ToString' should be 'String'"});
	CHECK(check("macro", "TOSTRING", props) == Lines{"naming.rename: macro 'TOSTRING' should be 'STRING'"});
	CHECK(check("local", "indexer", props).empty());
	CHECK(check("local", "ptr", props).empty());
}

TEST_CASE("checkNaming reports every broken rule in a fixed order")
{
	Properties props =
	{
		{"name", {"camelCase"}},
		{"banned-names", {"Host_Index_Of"}},
		{"banned-parts", {"Host"}},
		{"banned-prefixes", {"host"}},
		{"banned-suffixes", {"of"}},
		{"rename", {"index=idx"}}
	};

	CHECK(check("local", "Host_Index_Of", props) == Lines{
		"naming.case: local 'Host_Index_Of' should be camelCase",
		"naming.banned: local 'Host_Index_Of' is a banned name",
		"naming.banned: local 'Host_Index_Of' contains banned part 'Host'",
		"naming.prefix: local 'Host_Index_Of' starts with banned prefix 'host'",
		"naming.suffix: local 'Host_Index_Of' ends with banned suffix 'of'",
		"naming.rename: local 'Host_Index_Of' should be 'Host_Idx_Of'"});

	CHECK(check("local", "anything", {}).empty());
	CHECK(check("local", "_", {{"banned-parts", {"x"}}, {"rename", {"a=b"}}}).empty());
}

TEST_CASE("checkNaming accepts any of several styles and names them all when none fits")
{
	Properties props = {{"name", {"UPPER_SNAKE", "LIB_PascalCase"}}, {"banned-parts", {"signal"}}};

	CHECK(check("macro", "MAX_SIZE", props).empty());
	CHECK(check("macro", "LIB_Action", props).empty());
	CHECK(check("macro", "badMacro", props) == Lines{"naming.case: macro 'badMacro' should be UPPER_SNAKE or LIB_PascalCase"});
	CHECK(check("macro", "LIB_Signal", props) == Lines{"naming.banned: macro 'LIB_Signal' contains banned part 'signal'"});

	Properties members = {{"name", {"m_PascalCase", "s_PascalCase"}}, {"rename", {"index=idx"}}};

	CHECK(check("member", "s_Index", members) == Lines{"naming.rename: member 's_Index' should be 's_Idx'"});
	CHECK(check("member", "m_Index", members) == Lines{"naming.rename: member 'm_Index' should be 'm_Idx'"});
}

TEST_CASE("checkNaming reports plural last words unless the ending or the allow list says otherwise")
{
	Properties props = {{"name", {"PascalCase"}}, {"plural", {"report"}}, {"plural-allow", {"Options", "Canvas"}}};

	CHECK(check("type", "EnemyManagers", props) == Lines{"naming.plural: type 'EnemyManagers' should be singular"});
	CHECK(check("type", "Items", props) == Lines{"naming.plural: type 'Items' should be singular"});

	for (char const* name : {"EnemyManager", "Status", "Process", "Axis", "Physics", "Nervous", "PlayerOptions", "SGCanvas", "Gs"})
	{
		CHECK(check("type", name, props).empty());
	}

	CHECK(check("type", "Items", {{"plural", {"allow"}}}).empty());
	CHECK(check("type", "Items", {}).empty());
}
