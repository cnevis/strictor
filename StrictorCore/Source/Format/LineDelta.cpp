#include "Format/LineDelta.h"

#include <cstdint>

static std::vector<std::string> split(std::string const& text)
{
	std::vector<std::string> result;
	std::size_t pos = 0;

	while (pos < text.size())
	{
		std::size_t end = text.find('\n', pos);

		if (end == std::string::npos)
		{
			end = text.size();
		}

		std::string line = text.substr(pos, end - pos);

		if (not line.empty() and line.back() == '\r')
		{
			line.pop_back();
		}

		result.push_back(line);
		pos = end + 1;
	}

	return result;
}

LineDelta::LineDelta(std::string const& base, std::string const& text)
{
	std::vector<std::string> a = split(base);
	std::vector<std::string> b = split(text);
	std::size_t head = 0;
	std::size_t tail = 0;

	lines.assign(b.size(), true);

	while (head < a.size() and head < b.size() and a[head] == b[head])
	{
		lines[head] = false;
		head++;
	}

	while (tail < a.size() - head and tail < b.size() - head and a[a.size() - 1 - tail] == b[b.size() - 1 - tail])
	{
		lines[b.size() - 1 - tail] = false;
		tail++;
	}

	std::size_t n = a.size() - head - tail;
	std::size_t m = b.size() - head - tail;

	if (n == 0 or m == 0 or n * m > 64'000'000)
	{
		return;
	}

	std::vector<std::uint8_t> moves(n * m);
	std::vector<int> prev(m + 1, 0);
	std::vector<int> cur(m + 1, 0);

	for (std::size_t i = 0; i < n; i++)
	{
		for (std::size_t j = 0; j < m; j++)
		{
			if (a[head + i] == b[head + j])
			{
				cur[j + 1] = prev[j] + 1;
				moves[i * m + j] = 0;
			}
			else if (prev[j + 1] >= cur[j])
			{
				cur[j + 1] = prev[j + 1];
				moves[i * m + j] = 1;
			}
			else
			{
				cur[j + 1] = cur[j];
				moves[i * m + j] = 2;
			}
		}

		std::swap(prev, cur);
	}

	std::size_t i = n;
	std::size_t j = m;

	while (i > 0 and j > 0)
	{
		std::uint8_t move = moves[(i - 1) * m + (j - 1)];

		if (move == 0)
		{
			lines[head + j - 1] = false;
			i--;
			j--;
		}
		else if (move == 1)
		{
			i--;
		}
		else
		{
			j--;
		}
	}
}

bool LineDelta::empty() const
{
	for (bool line : lines)
	{
		if (line)
		{
			return false;
		}
	}

	return true;
}

bool LineDelta::contains(int line) const
{
	return line >= 1 and line <= (int)lines.size() and lines[(std::size_t)line - 1];
}

std::vector<std::pair<int, int>> LineDelta::ranges() const
{
	std::vector<std::pair<int, int>> result;

	for (int line = 1; line <= (int)lines.size(); line++)
	{
		if (not contains(line))
		{
			continue;
		}

		if (not result.empty() and result.back().second == line - 1)
		{
			result.back().second = line;
		}
		else
		{
			result.push_back({line, line});
		}
	}

	return result;
}

void LineDelta::filter(SourceText const& src, std::vector<Edit>* edits) const
{
	std::vector<Edit> kept;

	for (Edit const& e : *edits)
	{
		int first = src.line(e.offset);
		int last = src.line(e.offset + e.length);

		if (e.offset == src.lineStart(first))
		{
			first--;
		}

		for (int line = first; line <= last; line++)
		{
			if (contains(line))
			{
				kept.push_back(e);
				break;
			}
		}
	}

	*edits = kept;
}

void LineDelta::filter(std::vector<Diagnostic>* diagnostics) const
{
	std::vector<Diagnostic> kept;

	for (Diagnostic const& d : *diagnostics)
	{
		if (contains(d.line))
		{
			kept.push_back(d);
		}
	}

	*diagnostics = kept;
}
