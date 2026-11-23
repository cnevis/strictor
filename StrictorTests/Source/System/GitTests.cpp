#include "Support/TempDir.h"
#include "Support/WorkingDir.h"
#include "System/Git.h"
#include "System/Process.h"

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <string>
#include <vector>

static void git(TempDir const& tmp, std::vector<std::string> const& args)
{
	ProcessResult r = runProcess("git", args, "", tmp.path());
	INFO(r.error);
	REQUIRE(r.exitCode == 0);
}

static std::set<std::string> generic(std::vector<std::filesystem::path> const& paths)
{
	std::set<std::string> s;

	for (std::filesystem::path const& p : paths)
	{
		s.insert(p.generic_string());
	}

	return s;
}

TEST_CASE("gitChangedFiles lists modified, staged, untracked and renamed files relative to the working directory")
{
	TempDir tmp;
	git(tmp, {"init", "-q"});
	tmp.write("keep.cpp", "keep\n");
	tmp.write("edit.cpp", "edit\n");
	tmp.write("old.cpp", "old\n");
	tmp.write("gone.cpp", "gone\n");
	tmp.write("sub/deep.h", "deep\n");
	git(tmp, {"add", "-A"});
	git(tmp, {"-c", "user.name=t", "-c", "user.email=t@t", "commit", "-qm", "init"});

	tmp.write("edit.cpp", "changed\n");
	tmp.write("sub/deep.h", "changed\n");
	tmp.write("new file.cpp", "new\n");
	tmp.write("sub/untracked/inner.cpp", "inner\n");
	tmp.write("staged.cpp", "staged\n");
	git(tmp, {"add", "staged.cpp"});
	git(tmp, {"mv", "old.cpp", "moved.cpp"});
	std::filesystem::remove(tmp.path() / "gone.cpp");

	std::set<std::string> expected =
	{
		"edit.cpp",
		"moved.cpp",
		"new file.cpp",
		"staged.cpp",
		"sub/deep.h",
		"sub/untracked/inner.cpp"
	};

	{
		WorkingDir wd(tmp.path());
		CHECK(generic(gitChangedFiles()) == expected);
	}

	{
		WorkingDir wd(tmp.path() / "sub");
		std::set<std::string> fromSub =
		{
			"../edit.cpp",
			"../moved.cpp",
			"../new file.cpp",
			"../staged.cpp",
			"deep.h",
			"untracked/inner.cpp"
		};
		CHECK(generic(gitChangedFiles()) == fromSub);
	}
}

TEST_CASE("gitChangedFiles returns nothing for a clean repository and fails outside one")
{
	TempDir tmp;
	tmp.write("a.cpp", "a\n");

	{
		WorkingDir wd(tmp.path());
		CHECK_THROWS_AS(gitChangedFiles(), std::runtime_error);
	}

	git(tmp, {"init", "-q"});
	git(tmp, {"add", "-A"});
	git(tmp, {"-c", "user.name=t", "-c", "user.email=t@t", "commit", "-qm", "init"});

	WorkingDir wd(tmp.path());
	CHECK(gitChangedFiles().empty());
}
