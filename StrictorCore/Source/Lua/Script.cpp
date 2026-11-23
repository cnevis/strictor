#include "Lua/Script.h"

#include "Style/Stylesheet.h"
#include "System/Text.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

static char const g_SourceKey[] = "strictor.source";

static void check(sol::protected_function_result const& r)
{
	if (not r.valid())
	{
		sol::error err = r;
		throw std::runtime_error(err.what());
	}
}

RuleContext::RuleContext(SourceText const& src, std::vector<Diagnostic>* diagnostics, std::vector<Edit>* edits)
	: src(src)
	, diagnostics(diagnostics)
	, edits(edits)
{
}

void RuleContext::report(std::string const& rule, SyntaxNode const& node, std::string const& msg)
{
	diagnostics->push_back({node.line, node.column, rule, msg});
}

void RuleContext::insertBefore(SyntaxNode const& node, std::string const& text)
{
	edits->push_back({node.start, 0, text});
}

SourceText const& RuleContext::source() const
{
	return src;
}

Script::Script(std::filesystem::path const& file)
{
	std::ifstream in(file, std::ios::binary);

	if (not in)
	{
		throw std::runtime_error("cannot read " + utf8(file));
	}

	std::ostringstream ss;
	ss << in.rdbuf();

	lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::math, sol::lib::table);

	lua.new_usertype<SyntaxNode>("SyntaxNode",
		sol::no_constructor,
		"type", sol::readonly_property([](SyntaxNode const& n)
		{
			return n.node.type;
		}),
		"name", sol::readonly(&SyntaxNode::name),
		"line", sol::readonly(&SyntaxNode::line),
		"column", sol::readonly(&SyntaxNode::column),
		"parent", sol::readonly_property([](SyntaxNode const& n)
		{
			return n.parent;
		}),
		"attributes", sol::readonly_property([](SyntaxNode const& n, sol::this_state s)
		{
			sol::table t = sol::state_view(s).create_table();

			for (auto const& [k, v] : n.node.attributes)
			{
				t[k] = v;
			}

			return t;
		}),
		"lines", [](SyntaxNode const& n)
		{
			return n.lastLine - n.firstLine + 1;
		},
		"blankBefore", [](SyntaxNode const& n, sol::this_state s)
		{
			SourceText const* src = sol::state_view(s).registry()[g_SourceKey];

			return src->blankLinesAbove(n.firstLine);
		});

	lua.new_usertype<RuleContext>("RuleContext",
		sol::no_constructor,
		"report", &RuleContext::report,
		"insertBefore", &RuleContext::insertBefore);

	lua.set_function("on", [this](std::string const& type, sol::protected_function fn)
	{
		if (not validNodeType(type))
		{
			throw std::runtime_error("unknown node type '" + type + "'");
		}

		handlers[type].push_back(fn);
	});

	check(lua.safe_script(ss.str(), sol::script_pass_on_error, "@" + utf8(file)));
}

void Script::run(SyntaxNode const& node, RuleContext* ctx)
{
	std::map<std::string, std::vector<sol::protected_function>>::iterator it = handlers.find(node.node.type);

	if (it == handlers.end())
	{
		return;
	}

	lua.registry()[g_SourceKey] = &ctx->source();

	for (sol::protected_function& fn : it->second)
	{
		check(fn(&node, ctx));
	}
}
