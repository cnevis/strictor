#include "Style/Cascade.h"

#include <map>
#include <string>

void Cascade::append(Stylesheet const& sheet)
{
	for (Rule const& r : sheet.rules())
	{
		rules.push_back(r);
	}
}

Properties Cascade::compute(Node const& node) const
{
	Properties props;
	std::map<std::string, int> best;

	for (Rule const& r : rules)
	{
		int specificity = -1;

		for (Selector const& s : r.selectors)
		{
			if (s.matches(node) and s.specificity() > specificity)
			{
				specificity = s.specificity();
			}
		}

		if (specificity < 0)
		{
			continue;
		}

		for (Declaration const& d : r.declarations)
		{
			std::map<std::string, int>::const_iterator it = best.find(d.property);

			if (it == best.end() or specificity >= it->second)
			{
				best[d.property] = specificity;
				props[d.property] = d.values;
			}
		}
	}

	return props;
}
