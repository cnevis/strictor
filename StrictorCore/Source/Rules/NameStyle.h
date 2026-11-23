#pragma once

#include <string>

class NameStyle
{
public:
	explicit NameStyle(std::string const& spec);

	bool matches(std::string const& name) const;
	std::string const& prefix() const;

private:
	enum class LetterCase
	{
		PASCAL,
		CAMEL,
		UPPER_SNAKE,
		SNAKE
	};

	std::string stylePrefix;
	LetterCase letterCase = LetterCase::CAMEL;
};
