#include "App/Request.h"
#include "App/Runner.h"
#include "Clang/Toolchain.h"

#include <exception>
#include <print>

int wmain(int argc, wchar_t** argv)
{
	try
	{
		Request req = parseRequest(argc, argv);
		Toolchain tc;
		Runner runner(tc, req.buildDir, req.whole);

		switch (req.verb)
		{
			case Verb::FILE:
			{
				runner.files(req.paths);
				break;
			}

			case Verb::CHANGED:
			{
				runner.changed();
				break;
			}

			case Verb::STDIN:
			{
				runner.stream(req.paths[0]);
				break;
			}
		}

		return runner.exitCode();
	}
	catch (std::exception const& e)
	{
		std::println(stderr, "strictor: {}", e.what());
		return 2;
	}
}
