#include "Clang/CompileDatabase.h"

#include "Clang/ClangString.h"
#include "System/Text.h"

#include <cwchar>
#include <stdexcept>

static bool samePath(std::filesystem::path const& a, std::filesystem::path const& b)
{
	return _wcsicmp(a.lexically_normal().c_str(), b.lexically_normal().c_str()) == 0;
}

static bool clMode(std::string const& driver)
{
	std::filesystem::path stem = utf8Path(driver).stem();

	return _wcsicmp(stem.c_str(), L"cl") == 0 or _wcsicmp(stem.c_str(), L"clang-cl") == 0;
}

static bool cSource(std::filesystem::path const& file)
{
	return _wcsicmp(file.extension().c_str(), L".c") == 0;
}

CompileDatabase::CompileDatabase(std::filesystem::path const& dir)
{
	if (not std::filesystem::exists(dir / "compile_commands.json"))
	{
		throw std::runtime_error("no compile_commands.json in " + utf8(dir));
	}

	CXCompilationDatabase_Error error = CXCompilationDatabase_NoError;
	db = clang_CompilationDatabase_fromDirectory(utf8(dir).c_str(), &error);

	if (error != CXCompilationDatabase_NoError)
	{
		clang_CompilationDatabase_dispose(db);
		throw std::runtime_error("cannot load compile_commands.json from " + utf8(dir));
	}
}

CompileDatabase::~CompileDatabase()
{
	clang_CompilationDatabase_dispose(db);
}

CompileCommand CompileDatabase::command(std::filesystem::path const& file) const
{
	CXCompileCommands cmds = clang_CompilationDatabase_getCompileCommands(db, utf8(file).c_str());

	if (clang_CompileCommands_getSize(cmds) == 0)
	{
		clang_CompileCommands_dispose(cmds);
		return {};
	}

	CXCompileCommand cmd = clang_CompileCommands_getCommand(cmds, 0);
	CompileCommand cc;
	cc.dir = utf8Path(takeString(clang_CompileCommand_getDirectory(cmd)));

	std::filesystem::path source = cc.dir / utf8Path(takeString(clang_CompileCommand_getFilename(cmd)));
	unsigned n = clang_CompileCommand_getNumArgs(cmd);
	bool cl = n > 0 and clMode(takeString(clang_CompileCommand_getArg(cmd, 0)));

	if (cl)
	{
		cc.args.push_back("--driver-mode=cl");
	}

	for (unsigned i = 1; i < n; i++)
	{
		std::string arg = takeString(clang_CompileCommand_getArg(cmd, i));

		if (arg == "--")
		{
			break;
		}

		if (samePath(cc.dir / utf8Path(arg), source))
		{
			continue;
		}

		cc.args.push_back(arg);
	}

	clang_CompileCommands_dispose(cmds);

	if (cl)
	{
		cc.args.push_back(cSource(file) ? "/TC" : "/TP");
	}
	else
	{
		cc.args.push_back(cSource(file) ? "-xc" : "-xc++");
	}

	return cc;
}
