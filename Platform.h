#pragma once

// -----------------------------------------------------------------------------------------------------
// Platform compatibility layer.
//
// The editor is written against the MSVC CRT and Win32 on Windows, but the portable part of the code
// only relies on a handful of MSVC conveniences: the *_s string functions, _wfopen_s, _stricmp,
// sscanf_s, gets_s and __argc/__wargv. Everything else already goes through SDL2, zlib,
// std::filesystem and the project's own File abstraction.
//
// This header defines those conveniences (and the couple of helpers the port needs) so the very same
// sources build natively on Linux with GCC or Clang. On Windows it defines nothing: the CRT and the
// Win32 headers already provide all of it.
//
// Note on the *_s shims below: unlike the MSVC versions they truncate instead of running the invalid
// parameter handler, and they never write more than the buffer holds. The call sites in this project
// ignore the return value, so the behaviour is equivalent in practice.
// -----------------------------------------------------------------------------------------------------

#ifndef _WIN32

#include <cerrno>
#include <csignal>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <limits.h>
#include <string>
#include <string_view>
#include <strings.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#ifndef MAX_PATH
#define MAX_PATH PATH_MAX
#endif

typedef int errno_t;

#define _TRUNCATE ((size_t)-1)

#define _countof(array) (sizeof(array) / sizeof((array)[0]))

namespace xxl {

// ----- UTF-8 <-> wide conversions ------------------------------------------------------------------
// On Windows wchar_t is a UTF-16 code unit, on Linux it is a full Unicode code point (UTF-32).
// These two helpers are the ones the editor uses for its own text conversions on Linux.

inline std::string narrowFromWide(std::wstring_view text)
{
	std::string out;
	out.reserve(text.size());
	for (size_t i = 0; i < text.size(); i++) {
		uint32_t code = (uint32_t)text[i];
		// Wide strings coming from UTF-16 sources may hold surrogate pairs.
		if (code >= 0xD800 && code <= 0xDBFF && i + 1 < text.size()) {
			uint32_t low = (uint32_t)text[i + 1];
			if (low >= 0xDC00 && low <= 0xDFFF) {
				code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
				i++;
			}
		}
		if (code < 0x80) {
			out.push_back((char)code);
		}
		else if (code < 0x800) {
			out.push_back((char)(0xC0 | (code >> 6)));
			out.push_back((char)(0x80 | (code & 0x3F)));
		}
		else if (code < 0x10000) {
			out.push_back((char)(0xE0 | (code >> 12)));
			out.push_back((char)(0x80 | ((code >> 6) & 0x3F)));
			out.push_back((char)(0x80 | (code & 0x3F)));
		}
		else if (code <= 0x10FFFF) {
			out.push_back((char)(0xF0 | (code >> 18)));
			out.push_back((char)(0x80 | ((code >> 12) & 0x3F)));
			out.push_back((char)(0x80 | ((code >> 6) & 0x3F)));
			out.push_back((char)(0x80 | (code & 0x3F)));
		}
		else {
			out.push_back('?');
		}
	}
	return out;
}

inline std::wstring widenFromUtf8(std::string_view text)
{
	std::wstring out;
	out.reserve(text.size());
	size_t i = 0;
	while (i < text.size()) {
		unsigned char byte = (unsigned char)text[i];
		uint32_t code = 0xFFFD;
		size_t length = 1;
		if (byte < 0x80) {
			code = byte;
		}
		else if ((byte & 0xE0) == 0xC0 && i + 1 < text.size()) {
			code = ((uint32_t)(byte & 0x1F) << 6) | ((uint32_t)(unsigned char)text[i + 1] & 0x3F);
			length = 2;
		}
		else if ((byte & 0xF0) == 0xE0 && i + 2 < text.size()) {
			code = ((uint32_t)(byte & 0x0F) << 12) | (((uint32_t)(unsigned char)text[i + 1] & 0x3F) << 6) | ((uint32_t)(unsigned char)text[i + 2] & 0x3F);
			length = 3;
		}
		else if ((byte & 0xF8) == 0xF0 && i + 3 < text.size()) {
			code = ((uint32_t)(byte & 0x07) << 18) | (((uint32_t)(unsigned char)text[i + 1] & 0x3F) << 12) | (((uint32_t)(unsigned char)text[i + 2] & 0x3F) << 6) | ((uint32_t)(unsigned char)text[i + 3] & 0x3F);
			length = 4;
		}
		i += length;
		out.push_back((wchar_t)code);
	}
	return out;
}

// ----- executable location (replaces GetModuleHandle / GetModuleFileName) --------------------------

inline std::filesystem::path getExecutablePath()
{
	char buffer[4096];
	ssize_t length = ::readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
	if (length <= 0)
		return {};
	buffer[length] = 0;
	return std::filesystem::path(buffer);
}

inline std::filesystem::path getExecutableDirectory()
{
	std::filesystem::path path = getExecutablePath();
	return path.empty() ? std::filesystem::path{} : path.parent_path();
}

// ----- running other programs (file dialogs, the game launcher) ------------------------------------

inline bool commandExists(const std::string& name)
{
	const char* pathEnv = getenv("PATH");
	if (!pathEnv)
		return false;
	std::string paths(pathEnv);
	size_t start = 0;
	while (start <= paths.size()) {
		size_t end = paths.find(':', start);
		std::string dir = (end == std::string::npos) ? paths.substr(start) : paths.substr(start, end - start);
		if (!dir.empty()) {
			std::filesystem::path candidate = std::filesystem::path(dir) / name;
			if (access(candidate.c_str(), X_OK) == 0)
				return true;
		}
		if (end == std::string::npos)
			break;
		start = end + 1;
	}
	return false;
}

// Runs a program, captures its standard output when asked for, and returns its exit code
// (-1 when the program could not be started).
inline int runCommand(const std::vector<std::string>& arguments, std::string* output)
{
	int pipefd[2];
	if (output && pipe(pipefd) != 0)
		return -1;
	pid_t pid = fork();
	if (pid < 0) {
		if (output) {
			close(pipefd[0]);
			close(pipefd[1]);
		}
		return -1;
	}
	if (pid == 0) {
		if (output) {
			dup2(pipefd[1], STDOUT_FILENO);
			close(pipefd[0]);
			close(pipefd[1]);
		}
		std::vector<char*> argv;
		argv.reserve(arguments.size() + 1);
		for (const std::string& argument : arguments)
			argv.push_back(const_cast<char*>(argument.c_str()));
		argv.push_back(nullptr);
		execvp(argv[0], argv.data());
		_exit(127);
	}
	if (output) {
		close(pipefd[1]);
		char buffer[4096];
		ssize_t length;
		while ((length = read(pipefd[0], buffer, sizeof(buffer))) > 0)
			output->append(buffer, (size_t)length);
		close(pipefd[0]);
	}
	int status = 0;
	waitpid(pid, &status, 0);
	if (WIFEXITED(status))
		return WEXITSTATUS(status);
	return -1;
}

} // namespace xxl

// ----- string formatting (MSVC *_s family) ---------------------------------------------------------

template <size_t N>
inline int sprintf_s(char (&buffer)[N], const char* format, ...)
{
	va_list args;
	va_start(args, format);
	int result = vsnprintf(buffer, N, format, args);
	va_end(args);
	return result;
}

inline int sprintf_s(char* buffer, size_t sizeOfBuffer, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	int result = vsnprintf(buffer, sizeOfBuffer, format, args);
	va_end(args);
	return result;
}

template <size_t N>
inline int _snprintf_s(char (&buffer)[N], size_t count, const char* format, ...)
{
	(void)count; // _TRUNCATE: the buffer size is the only limit
	va_list args;
	va_start(args, format);
	int result = vsnprintf(buffer, N, format, args);
	va_end(args);
	return result;
}

template <size_t N>
inline int swprintf_s(wchar_t (&buffer)[N], const wchar_t* format, ...)
{
	va_list args;
	va_start(args, format);
	int result = vswprintf(buffer, N, format, args);
	va_end(args);
	return result;
}

inline int swprintf_s(wchar_t* buffer, size_t sizeOfBuffer, const wchar_t* format, ...)
{
	va_list args;
	va_start(args, format);
	int result = vswprintf(buffer, sizeOfBuffer, format, args);
	va_end(args);
	return result;
}

template <size_t N>
inline int vswprintf_s(wchar_t (&buffer)[N], const wchar_t* format, va_list args)
{
	return vswprintf(buffer, N, format, args);
}

inline int vswprintf_s(wchar_t* buffer, size_t sizeOfBuffer, const wchar_t* format, va_list args)
{
	return vswprintf(buffer, sizeOfBuffer, format, args);
}

inline int vsprintf_s(char* buffer, size_t sizeOfBuffer, const char* format, va_list args)
{
	return vsnprintf(buffer, sizeOfBuffer, format, args);
}

template <size_t N>
inline int vsprintf_s(char (&buffer)[N], const char* format, va_list args)
{
	return vsnprintf(buffer, N, format, args);
}

// ----- bounded string copies ----------------------------------------------------------------------

inline errno_t strcpy_s(char* dest, size_t destSize, const char* src)
{
	if (!dest || destSize == 0)
		return EINVAL;
	size_t length = src ? strlen(src) : 0;
	if (length + 1 > destSize) {
		dest[0] = 0;
		return ERANGE;
	}
	if (length)
		memcpy(dest, src, length);
	dest[length] = 0;
	return 0;
}

template <size_t N>
inline errno_t strcpy_s(char (&dest)[N], const char* src)
{
	return strcpy_s(dest, N, src);
}

inline errno_t strncpy_s(char* dest, size_t destSize, const char* src, size_t count)
{
	if (!dest || destSize == 0)
		return EINVAL;
	size_t length = src ? strnlen(src, count) : 0;
	if (length + 1 > destSize) {
		dest[0] = 0;
		return ERANGE;
	}
	if (length)
		memcpy(dest, src, length);
	dest[length] = 0;
	return 0;
}

template <size_t N>
inline errno_t strncpy_s(char (&dest)[N], const char* src, size_t count)
{
	return strncpy_s(dest, N, src, count);
}

inline errno_t strcat_s(char* dest, size_t destSize, const char* src)
{
	if (!dest || destSize == 0)
		return EINVAL;
	size_t destLength = strlen(dest);
	size_t srcLength = src ? strlen(src) : 0;
	if (destLength + srcLength + 1 > destSize) {
		dest[0] = 0;
		return ERANGE;
	}
	memcpy(dest + destLength, src, srcLength + 1);
	return 0;
}

template <size_t N>
inline errno_t strcat_s(char (&dest)[N], const char* src)
{
	return strcat_s(dest, N, src);
}

// ----- file streams --------------------------------------------------------------------------------

inline errno_t fopen_s(FILE** streamptr, const char* filename, const char* mode)
{
	if (!streamptr || !filename || !mode)
		return EINVAL;
	*streamptr = fopen(filename, mode);
	return *streamptr ? 0 : errno;
}

inline errno_t _wfopen_s(FILE** streamptr, const char* filename, const wchar_t* mode)
{
	std::string narrowMode = xxl::narrowFromWide(mode ? std::wstring_view(mode) : std::wstring_view());
	return fopen_s(streamptr, filename, narrowMode.c_str());
}

inline errno_t _wfopen_s(FILE** streamptr, const wchar_t* filename, const wchar_t* mode)
{
	std::string narrowName = xxl::narrowFromWide(filename ? std::wstring_view(filename) : std::wstring_view());
	return _wfopen_s(streamptr, narrowName.c_str(), mode);
}

inline errno_t _wfopen_s(FILE** streamptr, const std::filesystem::path& filename, const wchar_t* mode)
{
	return _wfopen_s(streamptr, filename.string().c_str(), mode);
}

// ----- misc CRT conveniences -----------------------------------------------------------------------

inline int _stricmp(const char* a, const char* b)
{
	return ::strcasecmp(a, b);
}

inline char* strtok_s(char* str, const char* delimiters, char** context)
{
	return strtok_r(str, delimiters, context);
}

inline int _wcsicmp(const wchar_t* a, const wchar_t* b)
{
	return ::wcscasecmp(a, b);
}

inline int sscanf_s(const char* buffer, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	int result = vsscanf(buffer, format, args);
	va_end(args);
	return result;
}

template <size_t N>
inline char* gets_s(char (&buffer)[N])
{
	if (!fgets(buffer, (int)N, stdin))
		return nullptr;
	size_t length = strlen(buffer);
	if (length && buffer[length - 1] == '\n')
		buffer[length - 1] = 0;
	return buffer;
}

inline void DebugBreak()
{
	raise(SIGTRAP);
}

#endif // !_WIN32
