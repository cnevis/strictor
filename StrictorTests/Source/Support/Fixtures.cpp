#include "Support/Fixtures.h"

#include <windows.h>

Toolchain const& toolchain()
{
	static Toolchain const tc;

	return tc;
}

std::filesystem::path strictorExe()
{
	wchar_t buf[MAX_PATH];
	GetModuleFileNameW(nullptr, buf, MAX_PATH);

	return std::filesystem::path(buf).parent_path() / "strictor.exe";
}
