#include "Rules/Naming.h"

#include "Rules/NameStyle.h"
#include "System/Text.h"

#include <format>

struct Word
{
	std::size_t start;
	std::size_t size;
};

static bool boundary(std::string const& s, std::size_t i)
{
	char p = s[i - 1];
	char c = s[i];

	if ((lowerLetter(p) or digit(p)) and upperLetter(c))
	{
		return true;
	}

	return upperLetter(p) and upperLetter(c) and i + 1 < s.size() and lowerLetter(s[i + 1]);
}

static std::vector<Word> words(std::string const& s)
{
	std::vector<Word> result;
	std::size_t start = 0;

	for (std::size_t i = 0; i <= s.size(); i++)
	{
		if (i == s.size() or s[i] == '_')
		{
			if (i > start)
			{
				result.push_back({start, i - start});
			}

			start = i + 1;
			continue;
		}

		if (i > start and boundary(s, i))
		{
			result.push_back({start, i - start});
			start = i;
		}
	}

	return result;
}

static bool same(std::string const& a, std::string const& b)
{
	return lowercase(a) == lowercase(b);
}

static std::string matchCase(std::string const& text, std::string const& word)
{
	std::string s = text;

	if (word.size() > 1 and uppercase(word) == word)
	{
		return uppercase(s);
	}

	if (s.empty())
	{
		return s;
	}

	std::string first = s.substr(0, 1);
	s.replace(0, 1, upperLetter(word[0]) ? uppercase(first) : lowercase(first));

	return s;
}

static std::vector<std::string> const& values(Properties const& props, char const* key)
{
	static std::vector<std::string> const none;
	Properties::const_iterator it = props.find(key);

	return it == props.end() ? none : it->second;
}

static bool plural(std::string const& word, std::vector<std::string> const& allow)
{
	for (std::string const& a : allow)
	{
		if (same(word, a))
		{
			return false;
		}
	}

	std::string w = lowercase(word);

	if (w.size() < 3 or w.back() != 's')
	{
		return false;
	}

	for (char const* ending : {"ss", "us", "is", "ics", "ous"})
	{
		if (w.ends_with(ending))
		{
			return false;
		}
	}

	return true;
}

static std::string rename(std::string const& core, std::vector<Word> const& ws, std::vector<std::string> const& pairs)
{
	for (std::string const& p : pairs)
	{
		std::size_t eq = p.find('=');

		if (same(core, p.substr(0, eq)))
		{
			return matchCase(p.substr(eq + 1), core);
		}
	}

	std::string s = core;

	for (std::size_t i = ws.size(); i-- > 0;)
	{
		std::string w = core.substr(ws[i].start, ws[i].size);

		for (std::string const& p : pairs)
		{
			std::size_t eq = p.find('=');

			if (same(w, p.substr(0, eq)))
			{
				s.replace(ws[i].start, ws[i].size, matchCase(p.substr(eq + 1), w));
				break;
			}
		}
	}

	return s;
}

void checkNaming(Identifier const& id, Properties const& props, std::vector<Diagnostic>* result)
{
	std::vector<std::string> const& styles = values(props, "name");
	std::string prefix;
	std::string core = id.name;

	if (not styles.empty())
	{
		bool fits = false;
		std::string choices;

		for (std::string const& s : styles)
		{
			NameStyle ns(s);
			choices += (choices.empty() ? "" : " or ") + s;

			if (ns.matches(id.name))
			{
				fits = true;
				prefix = ns.prefix();
				break;
			}

			if (prefix.empty() and id.name.starts_with(ns.prefix()))
			{
				prefix = ns.prefix();
			}
		}

		if (not fits)
		{
			result->push_back({id.line, id.column, "naming.case", std::format("{} '{}' should be {}", id.label, id.name, choices)});
		}

		core = id.name.substr(prefix.size());
	}

	for (std::string const& b : values(props, "banned-names"))
	{
		if (id.name == b)
		{
			result->push_back({id.line, id.column, "naming.banned", std::format("{} '{}' is a banned name", id.label, id.name)});
		}
	}

	std::vector<Word> ws = words(core);

	if (ws.empty())
	{
		return;
	}

	for (Word const& w : ws)
	{
		std::string part = core.substr(w.start, w.size);

		for (std::string const& b : values(props, "banned-parts"))
		{
			if (same(part, b))
			{
				result->push_back({id.line, id.column, "naming.banned", std::format("{} '{}' contains banned part '{}'", id.label, id.name, b)});
			}
		}
	}

	std::string first = core.substr(ws.front().start, ws.front().size);
	std::string last = core.substr(ws.back().start, ws.back().size);

	for (std::string const& b : values(props, "banned-prefixes"))
	{
		if (same(first, b))
		{
			result->push_back({id.line, id.column, "naming.prefix", std::format("{} '{}' starts with banned prefix '{}'", id.label, id.name, b)});
		}
	}

	for (std::string const& b : values(props, "banned-suffixes"))
	{
		if (same(last, b))
		{
			result->push_back({id.line, id.column, "naming.suffix", std::format("{} '{}' ends with banned suffix '{}'", id.label, id.name, b)});
		}
	}

	std::vector<std::string> const& pluralMode = values(props, "plural");

	if (not pluralMode.empty() and pluralMode[0] == "report" and plural(last, values(props, "plural-allow")))
	{
		result->push_back({id.line, id.column, "naming.plural", std::format("{} '{}' should be singular", id.label, id.name)});
	}

	std::vector<std::string> const& pairs = values(props, "rename");

	if (pairs.empty())
	{
		return;
	}

	std::string suggestion = rename(core, ws, pairs);

	if (suggestion != core)
	{
		result->push_back({id.line, id.column, "naming.rename", std::format("{} '{}' should be '{}'", id.label, id.name, prefix + suggestion)});
	}
}
