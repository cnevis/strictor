#include "Clang/ClangString.h"

std::string takeString(CXString s)
{
	char const* c = clang_getCString(s);
	std::string str = c ? c : "";
	clang_disposeString(s);

	return str;
}
