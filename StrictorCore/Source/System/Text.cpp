#include "System/Text.h"

#include <windows.h>

std::string utf8(std::filesystem::path const& p)
{
	std::u8string s = p.u8string();

	return std::string((char const*)s.data(), s.size());
}

std::filesystem::path utf8Path(std::string const& s)
{
	return std::filesystem::path(std::u8string((char8_t const*)s.data(), s.size()));
}

std::wstring wide(std::string const& s)
{
	if (s.empty())
	{
		return {};
	}

	int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
	std::wstring w(n, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);

	return w;
}

std::string trim(std::string const& s)
{
	std::size_t first = s.find_first_not_of(" \t\r\n");

	if (first == std::string::npos)
	{
		return {};
	}

	std::size_t last = s.find_last_not_of(" \t\r\n");

	return s.substr(first, last - first + 1);
}

bool upperLetter(char c)
{
	return c >= 'A' and c <= 'Z';
}

bool lowerLetter(char c)
{
	return c >= 'a' and c <= 'z';
}

bool digit(char c)
{
	return c >= '0' and c <= '9';
}

std::string lowercase(std::string const& s)
{
	std::string r = s;

	for (char& c : r)
	{
		if (upperLetter(c))
		{
			c = (char)(c - 'A' + 'a');
		}
	}

	return r;
}

std::string uppercase(std::string const& s)
{
	std::string r = s;

	for (char& c : r)
	{
		if (lowerLetter(c))
		{
			c = (char)(c - 'a' + 'A');
		}
	}

	return r;
}

bool glob(std::string const& pattern, std::string const& s)
{
	std::size_t p = 0;
	std::size_t i = 0;
	std::size_t star = std::string::npos;
	std::size_t resume = 0;

	while (i < s.size())
	{
		if (p < pattern.size() and pattern[p] == '*')
		{
			star = p;
			resume = i;
			p++;
			continue;
		}

		if (p < pattern.size() and pattern[p] == s[i])
		{
			p++;
			i++;
			continue;
		}

		if (star == std::string::npos)
		{
			return false;
		}

		resume++;
		p = star + 1;
		i = resume;
	}

	while (p < pattern.size() and pattern[p] == '*')
	{
		p++;
	}

	return p == pattern.size();
}
