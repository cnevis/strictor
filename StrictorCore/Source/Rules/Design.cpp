#include "Rules/Design.h"

#include "System/Text.h"

#include <format>

static bool excused(FreeFunction const& f, Properties const& props)
{
	Properties::const_iterator allow = props.find("helper-allow");

	if (allow != props.end())
	{
		for (std::string const& pattern : allow->second)
		{
			if (glob(pattern, f.name))
			{
				return true;
			}
		}
	}

	Properties::const_iterator folders = props.find("helper-folders");

	if (folders == props.end())
	{
		return false;
	}

	for (std::filesystem::path const& part : f.path.parent_path())
	{
		for (std::string const& folder : folders->second)
		{
			if (lowercase(utf8(part)) == lowercase(folder))
			{
				return true;
			}
		}
	}

	return false;
}

void checkHelper(FreeFunction const& f, Properties const& props, std::vector<Diagnostic>* result)
{
	Properties::const_iterator helper = props.find("helper");

	if (helper == props.end() or helper->second[0] != "report" or excused(f, props))
	{
		return;
	}

	if (f.owner.empty())
	{
		result->push_back({f.line, f.column, "design.helper", std::format("free function '{}' should be a member function", f.name)});
		return;
	}

	std::string reasons;

	if (f.staticInClassFile)
	{
		reasons = "static in its .cpp";
	}

	if (f.oneClass)
	{
		reasons += (reasons.empty() ? "" : ", ") + std::string("called only by ") + f.owner;
	}

	std::string msg = std::format("free function '{}' belongs in class '{}'", f.name, f.owner);

	if (not reasons.empty())
	{
		msg += " (" + reasons + ")";
	}

	result->push_back({f.line, f.column, "design.helper", msg});
}
