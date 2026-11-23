#include "Clang/Toolchain.h"

#include "System/Text.h"

#include <windows.h>
#include <shlobj.h>

#include <stdexcept>

Toolchain::Toolchain()
{
	wchar_t* programFiles = nullptr;

	if (FAILED(SHGetKnownFolderPath(FOLDERID_ProgramFiles, 0, nullptr, &programFiles)))
	{
		CoTaskMemFree(programFiles);
		throw std::runtime_error("cannot locate Program Files");
	}

	bin = std::filesystem::path(programFiles) / "LLVM" / "bin";
	CoTaskMemFree(programFiles);

	if (not std::filesystem::exists(bin / "libclang.dll"))
	{
		throw std::runtime_error("libclang.dll not found in " + utf8(bin));
	}

	if (not std::filesystem::exists(clangFormat()))
	{
		throw std::runtime_error("clang-format.exe not found in " + utf8(bin));
	}

	SetDllDirectoryW(bin.c_str());
}

std::filesystem::path Toolchain::clangFormat() const
{
	return bin / "clang-format.exe";
}
