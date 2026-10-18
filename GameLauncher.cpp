#include "GameLauncher.h"
#include "Platform.h"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <fcntl.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string>
#include <vector>
#endif

static void sleepMs(int milliseconds)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

#ifdef _WIN32

bool GameLauncher::openGame()
{
	STARTUPINFOW startInfo;
	PROCESS_INFORMATION procInfo;
	ZeroMemory(&startInfo, sizeof(startInfo));
	ZeroMemory(&procInfo, sizeof(procInfo));
	startInfo.cb = sizeof(startInfo);
	namespace fs = std::filesystem;
	if (!CreateProcessW(fs::u8path(modulePath).c_str(), NULL, NULL, NULL, FALSE, 0, NULL, fs::u8path(gamePath).c_str(), &startInfo, &procInfo)) {
		return false;
	}
	this->processHandle = procInfo.hProcess;
	this->threadHandle = procInfo.hThread;
	return true;
}

void GameLauncher::closeGame()
{
	if (processHandle) {
		CloseHandle(processHandle);
		CloseHandle(threadHandle);
		processHandle = nullptr;
		threadHandle = nullptr;
	}
}

bool GameLauncher::isGameRunning()
{
	DWORD exitCode;
	if(processHandle)
		if (GetExitCodeProcess(processHandle, &exitCode))
			if (exitCode == STILL_ACTIVE)
				return true;
	return false;
}

bool GameLauncher::readMemory(uint32_t address, void* out, size_t length)
{
	return processHandle && ReadProcessMemory((HANDLE)processHandle, (LPCVOID)(uintptr_t)address, out, length, NULL) != FALSE;
}

bool GameLauncher::writeMemory(uint32_t address, const void* data, size_t length)
{
	return processHandle && WriteProcessMemory((HANDLE)processHandle, (LPVOID)(uintptr_t)address, data, length, NULL) != FALSE;
}

static BOOL CALLBACK EnumCallback(HWND hWnd, LPARAM lParam)
{
	GameLauncher *gl = (GameLauncher*)lParam;
	DWORD pid;
	GetWindowThreadProcessId(hWnd, &pid);
	if (pid == GetProcessId(gl->processHandle)) {
		gl->windowHandle = hWnd;
		return FALSE;
	}
	return TRUE;
}

void GameLauncher::raiseGameWindow()
{
	windowHandle = nullptr;
	EnumWindows(EnumCallback, (LPARAM)this);
	if (windowHandle) {
		HWND wnd = (HWND)windowHandle;
		BringWindowToTop(wnd);
		OpenIcon(wnd);
	}
}

#else

namespace {

// The game modules are Windows executables, so they are started through Wine when it is installed.
bool looksLikeWindowsExecutable(const std::filesystem::path& path)
{
	FILE* file = nullptr;
	if (fopen_s(&file, path.string().c_str(), "rb") != 0 || !file)
		return false;
	char signature[2] = { 0, 0 };
	size_t read = fread(signature, 1, 2, file);
	fclose(file);
	return read == 2 && signature[0] == 'M' && signature[1] == 'Z';
}

std::string procMemPath(pid_t pid, bool writable)
{
	return "/proc/" + std::to_string((long)pid) + "/mem";
}

} // namespace

bool GameLauncher::openGame()
{
	std::filesystem::path module = std::filesystem::absolute(std::filesystem::path(modulePath));
	std::vector<std::string> arguments;
	if (looksLikeWindowsExecutable(module) && xxl::commandExists("wine")) {
		arguments.push_back("wine");
		arguments.push_back(module.string());
	}
	else {
		arguments.push_back(module.string());
	}

	pid_t pid = fork();
	if (pid < 0)
		return false;
	if (pid == 0) {
		if (!gamePath.empty() && chdir(gamePath.c_str()) != 0)
			_exit(127);
		std::vector<char*> argv;
		argv.reserve(arguments.size() + 1);
		for (const std::string& argument : arguments)
			argv.push_back(const_cast<char*>(argument.c_str()));
		argv.push_back(nullptr);
		execvp(argv[0], argv.data());
		_exit(127);
	}
	processHandle = (void*)(intptr_t)pid;
	return true;
}

void GameLauncher::closeGame()
{
	if (processHandle) {
		pid_t pid = (pid_t)(intptr_t)processHandle;
		int status = 0;
		waitpid(pid, &status, WNOHANG); // reap the child when it already exited; never block here
		processHandle = nullptr;
	}
	threadHandle = nullptr;
}

bool GameLauncher::isGameRunning()
{
	if (!processHandle)
		return false;
	pid_t pid = (pid_t)(intptr_t)processHandle;
	int status = 0;
	pid_t result = waitpid(pid, &status, WNOHANG);
	if (result == 0)
		return true;
	processHandle = nullptr; // the process ended, and is reaped now
	return false;
}

bool GameLauncher::readMemory(uint32_t address, void* out, size_t length)
{
	if (!processHandle)
		return false;
	pid_t pid = (pid_t)(intptr_t)processHandle;
	struct iovec local = { out, length };
	struct iovec remote = { (void*)(uintptr_t)address, length };
	if (process_vm_readv(pid, &local, 1, &remote, 1, 0) == (ssize_t)length)
		return true;
	// process_vm_readv may be unavailable or refused; /proc/<pid>/mem needs the same permission but is
	// checked differently, so it is worth trying as a fallback (it is how a debugger would read it).
	int fd = open(procMemPath(pid, false).c_str(), O_RDONLY);
	if (fd < 0)
		return false;
	ssize_t result = pread(fd, out, length, (off_t)address);
	close(fd);
	return result == (ssize_t)length;
}

bool GameLauncher::writeMemory(uint32_t address, const void* data, size_t length)
{
	if (!processHandle)
		return false;
	pid_t pid = (pid_t)(intptr_t)processHandle;
	struct iovec local = { const_cast<void*>((const void*)data), length };
	struct iovec remote = { (void*)(uintptr_t)address, length };
	if (process_vm_writev(pid, &local, 1, &remote, 1, 0) == (ssize_t)length)
		return true;
	int fd = open(procMemPath(pid, true).c_str(), O_RDWR);
	if (fd < 0)
		return false;
	ssize_t result = pwrite(fd, data, length, (off_t)address);
	close(fd);
	return result == (ssize_t)length;
}

void GameLauncher::raiseGameWindow()
{
	// Best effort: the game's window belongs to the game (or wine) process, so the window manager is
	// asked to activate it. xdotool is used when it is installed; otherwise nothing happens.
	if (!processHandle || !xxl::commandExists("xdotool"))
		return;
	pid_t pid = (pid_t)(intptr_t)processHandle;
	std::string output;
	if (xxl::runCommand({ "xdotool", "search", "--onlyvisible", "--pid", std::to_string((long)pid) }, &output) != 0)
		return;
	size_t end = output.find_first_of("\r\n");
	std::string windowId = (end == std::string::npos) ? output : output.substr(0, end);
	if (!windowId.empty())
		xxl::runCommand({ "xdotool", "windowactivate", "--sync", windowId }, nullptr);
}

#endif

bool GameLauncher::loadLevel(uint32_t lvlNum)
{
	static const uint32_t yellowPagePtr[8] = { 0, 0x6621F4, 0x663D04, 0x72D944, 0x765BF8, 0, 0, 0 };
	static const uint32_t ypGameMgrOffset[8] = { 0, 0x8c, 0x80, 0x88, 0x8c, 0, 0, 0 };
	static const uint32_t gmLevelOffset[8] = { 0, 8, 0x70, 0x158, 0x1A4, 0, 0, 0 };
	if (version < 0 || version >= 8 || yellowPagePtr[version] == 0) return false;

	if (!isGameRunning()) {
		closeGame();
		bool b = openGame();
		if (!b) return false;
	}
	uint32_t adYellowPages = 0, adGameManager = 0;
	// The game fills these pointers in once it has started, so poll until they hold a value.
	auto waitForPointer = [this](uint32_t address, uint32_t &value) {
		for (int attempt = 0; attempt < 120; attempt++) {
			if (readMemory(address, &value, 4) && value != 0)
				return true;
			if (!isGameRunning())
				return false;
			sleepMs(250);
		}
		return false;
	};
	if (!waitForPointer(yellowPagePtr[version], adYellowPages)) return false;
	if (!waitForPointer(adYellowPages + ypGameMgrOffset[version], adGameManager)) return false;
	if (!writeMemory(adGameManager + gmLevelOffset[version], &lvlNum, 4)) return false;

	// Bring window in front
	raiseGameWindow();
	return true;
}
