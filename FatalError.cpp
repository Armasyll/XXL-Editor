#include "FatalError.h"
#include "Platform.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <SDL2/SDL.h>
#endif

void FatalError(const wchar_t* message)
{
#ifdef _WIN32
	MessageBoxW(NULL, message, L"XXL Editor Failure", 16);
#else
	// Show the failure in a dialog when a display is reachable, and always report it on the console:
	// someone starting the editor from a terminal will look there.
	std::string narrow = xxl::narrowFromWide(message);
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "XXL Editor Failure", narrow.c_str(), nullptr);
	fprintf(stderr, "XXL Editor Failure: %s\n", narrow.c_str());
#endif
#ifndef XEC_RELEASE
	DebugBreak();
#endif
	exit(-3);
}

void FatalError(const std::wstring& message)
{
	FatalError(message.c_str());
}

void FatalErrorCFormat(const wchar_t* message, ...)
{
	va_list args;
	va_start(args, message);
	wchar_t buffer[1024];
	vswprintf_s(buffer, message, args);
	FatalError(buffer);
	va_end(args);
}
