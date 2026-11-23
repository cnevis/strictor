#include "Rules/NameStyle.h"

#include "System/Text.h"

#include <stdexcept>

static bool snake(std::string const& s, bool (*letter)(char))
{
	if (not letter(s.front()) or s.back() == '_')
	{
		return false;
	}

	for (std::size_t i = 0; i < s.size(); i++)
	{
		char c = s[i];

		if (c == '_')
		{
			if (s[i - 1] == '_')
			{
				return false;
			}

			continue;
		}

		if (not letter(c) and not digit(c))
		{
			return false;
		}
	}

	return true;
}

static bool camel(std::string const& s, bool (*first)(char))
{
	if (not first(s.front()))
	{
		return false;
	}

	for (char c : s)
	{
		if (not upperLetter(c) and not lowerLetter(c) and not digit(c))
		{
			return false;
		}
	}

	return true;
}

NameStyle::NameStyle(std::string const& spec)
{
	struct Suffix
	{
		char const* text;
		LetterCase letterCase;
	};

	Suffix const suffixes[] =
	{
		{"PascalCase", LetterCase::PASCAL},
		{"camelCase", LetterCase::CAMEL},
		{"UPPER_SNAKE", LetterCase::UPPER_SNAKE},
		{"snake_case", LetterCase::SNAKE}
	};

	for (Suffix const& s : suffixes)
	{
		if (spec.ends_with(s.text))
		{
			stylePrefix = spec.substr(0, spec.size() - std::char_traits<char>::length(s.text));
			letterCase = s.letterCase;

			for (char c : stylePrefix)
			{
				if (not upperLetter(c) and not lowerLetter(c) and not digit(c) and c != '_')
				{
					throw std::runtime_error("invalid name prefix in '" + spec + "'");
				}
			}

			return;
		}
	}

	throw std::runtime_error("unknown name style '" + spec + "'; use PascalCase, camelCase, UPPER_SNAKE or snake_case");
}

bool NameStyle::matches(std::string const& name) const
{
	if (not name.starts_with(stylePrefix) or name.size() == stylePrefix.size())
	{
		return false;
	}

	std::string rest = name.substr(stylePrefix.size());

	switch (letterCase)
	{
		case LetterCase::PASCAL:
		{
			return camel(rest, upperLetter);
		}

		case LetterCase::CAMEL:
		{
			return camel(rest, lowerLetter);
		}

		case LetterCase::UPPER_SNAKE:
		{
			return snake(rest, upperLetter);
		}

		case LetterCase::SNAKE:
		{
			return snake(rest, lowerLetter);
		}
	}

	return false;
}

std::string const& NameStyle::prefix() const
{
	return stylePrefix;
}
