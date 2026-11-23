#include "Style/Stylesheet.h"

#include "Rules/NameStyle.h"
#include "System/Text.h"

#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>

static std::vector<std::string> const g_Types =
{
	"*", "type", "function", "variable", "enumerator", "macro", "namespace",
	"block", "statement", "control", "condition", "expression", "return", "case"
};

static std::vector<std::string> const g_Properties =
{
	"name", "rename", "banned-names", "banned-parts", "banned-prefixes", "banned-suffixes",
	"blank-before", "blank-after", "max-clauses", "logical-operators", "script",
	"helper", "helper-folders", "helper-allow", "plural", "plural-allow", "declarators", "braces", "report"
};

static std::map<std::string, std::vector<std::string>> const g_Attributes =
{
	{"storage", {"local", "parameter", "template", "member", "global"}},
	{"access", {"public", "protected", "private"}},
	{"const", {"constexpr", "const", "none"}},
	{"keyword", {"if", "for", "while", "do", "switch", "try"}},
	{"braced", {"yes", "no"}},
	{"scope", {"free", "member"}},
	{"linkage", {"internal", "external"}},
	{"file", {"class", "other"}},
	{"callers", {"none", "one-class", "several"}},
	{"types", {"plain", "project"}},
	{"enum", {"class", "plain"}}
};

static bool number(std::string const& s, int least)
{
	if (s.empty() or s.size() > 3)
	{
		return false;
	}

	for (char c : s)
	{
		if (not digit(c))
		{
			return false;
		}
	}

	return std::stoi(s) >= least;
}

static bool matchPart(Selector const& s, std::size_t part, Node const* node)
{
	if (not s.parts[part].matches(*node))
	{
		return false;
	}

	if (part == 0)
	{
		return true;
	}

	if (s.combinators[part - 1] == Combinator::CHILD)
	{
		return node->parent and matchPart(s, part - 1, node->parent);
	}

	for (Node const* a = node->parent; a; a = a->parent)
	{
		if (matchPart(s, part - 1, a))
		{
			return true;
		}
	}

	return false;
}

bool validNodeType(std::string const& type)
{
	for (std::string const& t : g_Types)
	{
		if (t == type and t != "*")
		{
			return true;
		}
	}

	return false;
}

class StyleParser
{
public:
	StyleParser(std::filesystem::path const& file, std::string const& text)
		: file(file)
		, text(text)
	{
	}

	std::vector<Rule> parse()
	{
		std::vector<Rule> rules;
		skipSpace();

		while (pos < text.size())
		{
			rules.push_back(rule());
			skipSpace();
		}

		return rules;
	}

private:
	Rule rule()
	{
		Rule r;
		r.selectors.push_back(selector());
		skipSpace();

		while (peek() == ',')
		{
			pos++;
			skipSpace();
			r.selectors.push_back(selector());
			skipSpace();
		}

		if (peek() != '{')
		{
			fail(peek() == '\0' ? "expected '{'" : "expected ',' or '{'");
		}

		pos++;
		skipSpace();

		while (peek() != '}')
		{
			if (peek() == '\0')
			{
				fail("expected '}'");
			}

			r.declarations.push_back(declaration());
			skipSpace();
		}

		pos++;

		return r;
	}

	Selector selector()
	{
		Selector s;
		s.parts.push_back(compound());

		while (true)
		{
			std::size_t before = pos;
			skipSpace();

			if (peek() == '>')
			{
				pos++;
				skipSpace();
				s.combinators.push_back(Combinator::CHILD);
				s.parts.push_back(compound());
				continue;
			}

			if (pos > before and (peek() == '*' or wordChar(peek())))
			{
				s.combinators.push_back(Combinator::DESCENDANT);
				s.parts.push_back(compound());
				continue;
			}

			pos = before;
			break;
		}

		return s;
	}

	Compound compound()
	{
		Compound s;

		if (peek() == '*')
		{
			pos++;
			s.type = "*";
		}
		else
		{
			s.type = word();
		}

		if (not contains(g_Types, s.type))
		{
			fail("unknown selector '" + s.type + "'");
		}

		while (peek() == '[')
		{
			pos++;

			Condition c;
			c.attribute = word();
			expect('=');

			if (c.attribute == "name")
			{
				c.value = pattern();
				expect(']');
				s.conditions.push_back(c);
				continue;
			}

			c.value = word();
			expect(']');

			std::map<std::string, std::vector<std::string>>::const_iterator it = g_Attributes.find(c.attribute);

			if (it == g_Attributes.end())
			{
				fail("unknown attribute '" + c.attribute + "'");
			}

			if (not contains(it->second, c.value))
			{
				fail("unknown value '" + c.value + "' for attribute '" + c.attribute + "'");
			}

			s.conditions.push_back(c);
		}

		return s;
	}

	Declaration declaration()
	{
		Declaration d;
		d.property = word();

		if (not contains(g_Properties, d.property))
		{
			fail("unknown property '" + d.property + "'");
		}

		skipSpace();
		expect(':');
		skipSpace();

		while (peek() != ';' and peek() != '}')
		{
			if (peek() == '\0')
			{
				fail("expected ';'");
			}

			d.values.push_back(value());
			skipSpace();
		}

		if (peek() == ';')
		{
			pos++;
		}

		if (d.values.empty())
		{
			fail("property '" + d.property + "' needs a value");
		}

		validate(d);
		resolveScripts(&d);

		return d;
	}

	void validate(Declaration const& d)
	{
		if (d.property == "name")
		{
			for (std::string const& v : d.values)
			{
				try
				{
					NameStyle style(v);
				}
				catch (std::exception const& e)
				{
					fail(e.what());
				}
			}

			return;
		}

		if (d.property == "rename")
		{
			for (std::string const& v : d.values)
			{
				std::size_t eq = v.find('=');

				if (eq == std::string::npos or eq == 0 or eq + 1 == v.size())
				{
					fail("rename takes from=to pairs");
				}
			}

			return;
		}

		if (d.property == "blank-before" or d.property == "blank-after")
		{
			if (d.values.size() != 1 or not number(d.values[0], 0))
			{
				fail(d.property + " takes a number of lines");
			}

			return;
		}

		if (d.property == "max-clauses")
		{
			if (d.values.size() != 1 or not number(d.values[0], 1))
			{
				fail("max-clauses takes a number of at least 1");
			}

			return;
		}

		if (d.property == "logical-operators")
		{
			if (d.values.size() != 1 or (d.values[0] != "words" and d.values[0] != "symbols"))
			{
				fail("logical-operators takes words or symbols");
			}

			return;
		}

		if (d.property == "helper" or d.property == "plural")
		{
			if (d.values.size() != 1 or (d.values[0] != "report" and d.values[0] != "allow"))
			{
				fail(d.property + " takes report or allow");
			}

			return;
		}

		if (d.property == "declarators")
		{
			if (d.values.size() != 1 or (d.values[0] != "split" and d.values[0] != "keep"))
			{
				fail("declarators takes split or keep");
			}

			return;
		}

		if (d.property == "braces")
		{
			if (d.values.size() != 1 or (d.values[0] != "insert" and d.values[0] != "keep"))
			{
				fail("braces takes insert or keep");
			}
		}
	}

	void resolveScripts(Declaration* d)
	{
		if (d->property != "script")
		{
			return;
		}

		for (std::string& v : d->values)
		{
			std::filesystem::path p = (file.parent_path() / utf8Path(v)).lexically_normal();

			if (not std::filesystem::is_regular_file(p))
			{
				fail("script '" + v + "' not found");
			}

			v = utf8(p);
		}
	}

	std::string word()
	{
		std::size_t start = pos;

		while (pos < text.size() and wordChar(text[pos]))
		{
			pos++;
		}

		if (pos == start)
		{
			fail("expected a name");
		}

		return text.substr(start, pos - start);
	}

	std::string pattern()
	{
		std::size_t start = pos;

		while (pos < text.size() and patternChar(text[pos]))
		{
			pos++;
		}

		if (pos == start)
		{
			fail("expected a name pattern");
		}

		return text.substr(start, pos - start);
	}

	std::string value()
	{
		std::size_t start = pos;

		while (pos < text.size() and valueChar(text[pos]))
		{
			pos++;
		}

		if (pos == start)
		{
			fail("expected a value");
		}

		return text.substr(start, pos - start);
	}

	void expect(char c)
	{
		if (peek() != c)
		{
			fail(std::string("expected '") + c + "'");
		}

		pos++;
	}

	char peek() const
	{
		return pos < text.size() ? text[pos] : '\0';
	}

	void skipSpace()
	{
		while (pos < text.size())
		{
			if (space(text[pos]))
			{
				pos++;
				continue;
			}

			if (text.compare(pos, 2, "/*") == 0)
			{
				std::size_t end = text.find("*/", pos + 2);

				if (end == std::string::npos)
				{
					fail("unterminated comment");
				}

				pos = end + 2;
				continue;
			}

			break;
		}
	}

	[[noreturn]] void fail(std::string const& msg) const
	{
		int line = 1;

		for (std::size_t i = 0; i < pos and i < text.size(); i++)
		{
			if (text[i] == '\n')
			{
				line++;
			}
		}

		throw std::runtime_error(utf8(file) + ":" + std::to_string(line) + ": " + msg);
	}

	static bool space(char c)
	{
		return c == ' ' or c == '\t' or c == '\r' or c == '\n';
	}

	static bool wordChar(char c)
	{
		return upperLetter(c) or lowerLetter(c) or digit(c) or c == '-' or c == '_';
	}

	static bool patternChar(char c)
	{
		return wordChar(c) or c == '*' or c == '~';
	}

	static bool valueChar(char c)
	{
		return not space(c) and c != ';' and c != '}' and c != '{';
	}

	static bool contains(std::vector<std::string> const& list, std::string const& s)
	{
		for (std::string const& item : list)
		{
			if (s == item)
			{
				return true;
			}
		}

		return false;
	}

	std::filesystem::path file;
	std::string const& text;
	std::size_t pos = 0;
};

bool Compound::matches(Node const& node) const
{
	if (type != "*" and type != node.type)
	{
		return false;
	}

	for (Condition const& c : conditions)
	{
		if (c.attribute == "name")
		{
			if (node.name.empty() or not glob(c.value, node.name))
			{
				return false;
			}

			continue;
		}

		std::map<std::string, std::string>::const_iterator it = node.attributes.find(c.attribute);

		if (it == node.attributes.end() or it->second != c.value)
		{
			return false;
		}
	}

	return true;
}

int Compound::specificity() const
{
	int s = (int)conditions.size() * 10;

	if (type != "*")
	{
		s += 1;
	}

	return s;
}

bool Selector::matches(Node const& node) const
{
	return matchPart(*this, parts.size() - 1, &node);
}

int Selector::specificity() const
{
	int s = 0;

	for (Compound const& c : parts)
	{
		s += c.specificity();
	}

	return s;
}

Stylesheet::Stylesheet(std::filesystem::path const& file)
{
	std::ifstream in(file, std::ios::binary);

	if (not in)
	{
		throw std::runtime_error("cannot read " + utf8(file));
	}

	std::ostringstream ss;
	ss << in.rdbuf();

	std::string text = ss.str();
	StyleParser parser(file, text);
	list = parser.parse();
}

std::vector<Rule> const& Stylesheet::rules() const
{
	return list;
}
