#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

struct GameLauncher {
	std::string modulePath, gamePath;
	int version;

	void *processHandle = nullptr;
	void *threadHandle = nullptr;
	void *windowHandle = nullptr;

	GameLauncher(const std::string& modulePath, const std::string& gamePath, int version) :
		modulePath(modulePath), gamePath(gamePath), version(version) {}

	bool openGame();
	void closeGame();
	bool isGameRunning();
	bool loadLevel(uint32_t lvlNum);

	// The memory of the running game, through the platform's own API: ReadProcessMemory and
	// WriteProcessMemory on Windows, process_vm_readv/process_vm_writev (with a /proc/<pid>/mem
	// fallback) on Linux.
	bool readMemory(uint32_t address, void* out, size_t length);
	bool writeMemory(uint32_t address, const void* data, size_t length);
	// Best effort "bring the game's window to the front" after a level was loaded into it.
	void raiseGameWindow();
};
