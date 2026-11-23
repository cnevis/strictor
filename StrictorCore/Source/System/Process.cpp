#include "System/Process.h"

#include "System/Text.h"

#include <windows.h>

#include <algorithm>
#include <stdexcept>
#include <thread>

class Handle
{
public:
	Handle() = default;

	~Handle()
	{
		close();
	}

	Handle(Handle const&) = delete;
	Handle& operator=(Handle const&) = delete;

	HANDLE* put()
	{
		close();

		return &h;
	}

	HANDLE get() const
	{
		return h;
	}

	void close()
	{
		if (h)
		{
			CloseHandle(h);
			h = nullptr;
		}
	}

private:
	HANDLE h = nullptr;
};

static void createPipe(Handle* read, Handle* write, Handle* parentEnd)
{
	SECURITY_ATTRIBUTES sa = {sizeof(sa), nullptr, TRUE};

	if (not CreatePipe(read->put(), write->put(), &sa, 0))
	{
		throw std::runtime_error("cannot create a pipe");
	}

	SetHandleInformation(parentEnd->get(), HANDLE_FLAG_INHERIT, 0);
}

static void quote(std::wstring* cmd, std::wstring const& arg)
{
	if (not arg.empty() and arg.find_first_of(L" \t\n\v\"") == std::wstring::npos)
	{
		cmd->append(arg);
		return;
	}

	cmd->push_back(L'"');

	for (std::size_t i = 0; i < arg.size(); i++)
	{
		std::size_t slashes = 0;

		while (i < arg.size() and arg[i] == L'\\')
		{
			slashes++;
			i++;
		}

		if (i == arg.size())
		{
			cmd->append(slashes * 2, L'\\');
			break;
		}

		if (arg[i] == L'"')
		{
			cmd->append(slashes * 2 + 1, L'\\');
		}
		else
		{
			cmd->append(slashes, L'\\');
		}

		cmd->push_back(arg[i]);
	}

	cmd->push_back(L'"');
}

static std::string readPipe(HANDLE h)
{
	std::string data;
	char buf[4096];
	DWORD n = 0;

	while (ReadFile(h, buf, sizeof(buf), &n, nullptr) and n > 0)
	{
		data.append(buf, n);
	}

	return data;
}

static void writePipe(Handle* h, std::string const& data)
{
	std::size_t done = 0;

	while (done < data.size())
	{
		DWORD chunk = (DWORD)std::min<std::size_t>(data.size() - done, 65536);
		DWORD n = 0;

		if (not WriteFile(h->get(), data.data() + done, chunk, &n, nullptr))
		{
			break;
		}

		done += n;
	}

	h->close();
}

ProcessResult runProcess(std::filesystem::path const& exe,
                         std::vector<std::string> const& args,
                         std::string const& input,
                         std::filesystem::path const& dir)
{
	Handle inRead;
	Handle inWrite;
	Handle outRead;
	Handle outWrite;
	Handle errRead;
	Handle errWrite;

	createPipe(&inRead, &inWrite, &inWrite);
	createPipe(&outRead, &outWrite, &outRead);
	createPipe(&errRead, &errWrite, &errRead);

	std::wstring cmd;
	quote(&cmd, exe.wstring());

	for (std::string const& arg : args)
	{
		cmd.push_back(L' ');
		quote(&cmd, wide(arg));
	}

	STARTUPINFOW si = {};
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdInput = inRead.get();
	si.hStdOutput = outWrite.get();
	si.hStdError = errWrite.get();

	PROCESS_INFORMATION pi = {};
	wchar_t const* cwd = dir.empty() ? nullptr : dir.c_str();
	BOOL started = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, cwd, &si, &pi);

	inRead.close();
	outWrite.close();
	errWrite.close();

	if (not started)
	{
		throw std::runtime_error("cannot start " + utf8(exe));
	}

	Handle process;
	Handle thread;
	*process.put() = pi.hProcess;
	*thread.put() = pi.hThread;

	ProcessResult r;
	std::thread writer(writePipe, &inWrite, std::cref(input));
	std::thread errReader([&r, &errRead]
	{
		r.error = readPipe(errRead.get());
	});

	r.output = readPipe(outRead.get());
	writer.join();
	errReader.join();

	DWORD code = 0;
	WaitForSingleObject(process.get(), INFINITE);
	GetExitCodeProcess(process.get(), &code);
	r.exitCode = (int)code;

	return r;
}
