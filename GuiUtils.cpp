#include "GuiUtils.h"
#include "window.h"
#include "imgui/imgui.h"
#include <cstring>
#include <cstdio>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <commdlg.h>
#include <shlobj_core.h>
#else
#include <SDL2/SDL.h>
#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>
#endif

#ifdef _WIN32

static std::wstring simpleCharToWcharConvert(const char* str) {
	if (!str) return {};
	size_t len = strlen(str);
	return { str, str + len };
}

static std::wstring filterCharToWcharConvert(const char* str) {
	if (!str) return {};
	const char* ptr = str;
	// find a double null char
	while (*ptr || *(ptr + 1)) ptr++;
	return { str, ptr + 1 };
}

static UINT MsgBoxIconToWin32Flags(GuiUtils::MsgBoxIcon icon) {
	switch (icon) {
	case GuiUtils::MsgBoxIcon::Information: return MB_ICONINFORMATION;
	case GuiUtils::MsgBoxIcon::Warning: return MB_ICONWARNING;
	case GuiUtils::MsgBoxIcon::Error: return MB_ICONERROR;
	}
	return 0;
}

#endif

// ImGui InputCallback for std::string
int GuiUtils::IGStdStringInputCallback(ImGuiInputTextCallbackData* data) {
	if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
		std::string* str = (std::string*)data->UserData;
		str->resize(data->BufTextLen);
		data->Buf = (char*)str->data();
	}
	return 0;
}

#ifdef _WIN32

// ---------------------------------------------------------------- Windows ---

void GuiUtils::MsgBox_Ok(Window* window, const char* message, GuiUtils::MsgBoxIcon flags)
{
	MessageBoxA((HWND)window->getNativeWindow(), message, "XXL Editor", MsgBoxIconToWin32Flags(flags));
}

GuiUtils::MsgBoxButton GuiUtils::MsgBox_YesNo(Window* window, const char* message, GuiUtils::MsgBoxIcon flags)
{
	const int result = MessageBoxA((HWND)window->getNativeWindow(), message, "XXL Editor", MB_YESNO | MsgBoxIconToWin32Flags(flags));
	switch (result) {
	case IDYES: return MsgBoxButton::Yes;
	case IDNO: return MsgBoxButton::No;
	}
	return MsgBoxButton::Unknown;
}

std::filesystem::path GuiUtils::OpenDialogBox(Window* window, const char* filter, const char* defExt)
{
	wchar_t filepath[MAX_PATH + 1] = L"\0";
	OPENFILENAMEW ofn = {};
	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = (HWND)window->getNativeWindow();
	ofn.hInstance = GetModuleHandle(NULL);
	std::wstring wFilter = filterCharToWcharConvert(filter);
	ofn.lpstrFilter = wFilter.c_str();
	ofn.nFilterIndex = 0;
	ofn.lpstrFile = filepath;
	ofn.nMaxFile = sizeof(filepath);
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
	std::wstring wExt = simpleCharToWcharConvert(defExt);
	ofn.lpstrDefExt = wExt.c_str();
	if (GetOpenFileNameW(&ofn))
		return filepath;
	return {};
}

std::vector<std::filesystem::path> GuiUtils::MultiOpenDialogBox(Window* window, const char* filter, const char* defExt)
{
	wchar_t filepath[1025] = L"\0";
	OPENFILENAMEW ofn = {};
	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = (HWND)window->getNativeWindow();
	ofn.hInstance = GetModuleHandle(NULL);
	std::wstring wFilter = filterCharToWcharConvert(filter);
	ofn.lpstrFilter = wFilter.c_str();
	ofn.nFilterIndex = 0;
	ofn.lpstrFile = filepath;
	ofn.nMaxFile = sizeof(filepath);
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR | OFN_ALLOWMULTISELECT | OFN_EXPLORER;
	std::wstring wExt = simpleCharToWcharConvert(defExt);
	ofn.lpstrDefExt = wExt.c_str();
	if (GetOpenFileNameW(&ofn)) {
		std::wstring folder = filepath;
		const wchar_t* nextfile = filepath + folder.size() + 1;
		if (*nextfile) {
			std::vector<std::filesystem::path> list;
			while (*nextfile) {
				size_t len = wcslen(nextfile);
				list.emplace_back((folder + L'\\').append(std::wstring_view(nextfile, len)));
				nextfile += len + 1;
			}
			return list;
		}
		else {
			return { std::move(folder) };
		}
	}
	return {};
}

std::filesystem::path GuiUtils::SaveDialogBox(Window* window, const char* filter, const char* defExt, const std::filesystem::path& defName)
{
	wchar_t filepath[MAX_PATH + 1] = L"\0";
	if (!defName.empty())
		wcscpy_s(filepath, defName.c_str());
	OPENFILENAMEW ofn = {};
	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = (HWND)window->getNativeWindow();
	ofn.hInstance = GetModuleHandle(NULL);
	std::wstring wFilter = filterCharToWcharConvert(filter);
	ofn.lpstrFilter = wFilter.c_str();
	ofn.nFilterIndex = 0;
	ofn.lpstrFile = filepath;
	ofn.nMaxFile = sizeof(filepath);
	ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
	std::wstring wExt = simpleCharToWcharConvert(defExt);
	ofn.lpstrDefExt = wExt.c_str();
	if (GetSaveFileNameW(&ofn))
		return filepath;
	return {};
}

std::filesystem::path GuiUtils::SelectFolderDialogBox(Window* window, const char* text)
{
	wchar_t dirname[MAX_PATH + 1];
	BROWSEINFOW bri;
	memset(&bri, 0, sizeof(bri));
	bri.hwndOwner = (HWND)window->getNativeWindow();
	bri.pszDisplayName = dirname;
	std::wstring wText(text, text + strlen(text));
	bri.lpszTitle = wText.c_str();
	bri.ulFlags = BIF_USENEWUI | BIF_RETURNONLYFSDIRS;
	PIDLIST_ABSOLUTE pid = SHBrowseForFolderW(&bri);
	if (pid != NULL) {
		SHGetPathFromIDListW(pid, dirname);
		return dirname;
	}
	return {};
}

std::string GuiUtils::latinToUtf8(std::string_view text)
{
	// latin -> UTF-16
	int widesize = MultiByteToWideChar(1252, 0, text.data(), text.size(), NULL, 0);
	wchar_t* widename = (wchar_t*)alloca(2 * widesize);
	MultiByteToWideChar(1252, 0, text.data(), text.size(), widename, widesize);

	// UTF-16 -> UTF-8
	int u8size = WideCharToMultiByte(CP_UTF8, 0, widename, widesize, NULL, 0, NULL, NULL);
	char* u8name = (char*)alloca(u8size);
	WideCharToMultiByte(CP_UTF8, 0, widename, widesize, u8name, u8size, NULL, NULL);

	return std::string(u8name, u8size);
}

std::string GuiUtils::utf8ToLatin(std::string_view text)
{
	// UTF-8 -> UTF-16
	int widesize = MultiByteToWideChar(CP_UTF8, 0, text.data(), text.size(), NULL, 0);
	wchar_t* widename = (wchar_t*)alloca(2 * widesize);
	MultiByteToWideChar(CP_UTF8, 0, text.data(), text.size(), widename, widesize);

	// UTF-16 -> latin
	int latinsize = WideCharToMultiByte(1252, 0, widename, widesize, NULL, 0, NULL, NULL);
	char* latinname = (char*)alloca(latinsize);
	WideCharToMultiByte(1252, 0, widename, widesize, latinname, latinsize, "?", NULL);

	return std::string(latinname, latinsize);
}


std::string GuiUtils::wcharToUtf8(std::wstring_view text)
{
	// UTF-16 -> UTF-8
	int u8size = WideCharToMultiByte(CP_UTF8, 0, text.data(), text.size(), NULL, 0, NULL, NULL);
	char* u8name = (char*)alloca(u8size);
	WideCharToMultiByte(CP_UTF8, 0, text.data(), text.size(), u8name, u8size, NULL, NULL);
	return std::string(u8name, u8size);
}

std::wstring GuiUtils::utf8ToWchar(std::string_view text)
{
	int widesize = MultiByteToWideChar(CP_UTF8, 0, text.data(), text.size(), NULL, 0);
	wchar_t* widename = (wchar_t*)alloca(2 * widesize);
	MultiByteToWideChar(CP_UTF8, 0, text.data(), text.size(), widename, widesize);
	return std::wstring(widename, widesize);
}

errno_t GuiUtils::fsfopen_s(FILE** streamptr, const std::filesystem::path& filename, const char* mode)
{
	return _wfopen_s(streamptr, filename.c_str(), std::wstring(mode, mode + strlen(mode)).c_str());
}

#else

// ---------------------------------------------------------------- Linux / POSIX ---

namespace {

// The dialog filter argument keeps the Win32 format the call sites use:
// "Label\0*.ext\0Other label\0*.other\0\0".
struct DialogFilter {
	std::string label;
	std::string pattern;
};

std::vector<DialogFilter> parseFilterSpec(const char* filter)
{
	std::vector<DialogFilter> filters;
	if (!filter)
		return filters;
	const char* ptr = filter;
	while (*ptr) {
		std::string label = ptr;
		ptr += label.size() + 1;
		if (!*ptr)
			break;
		std::string pattern = ptr;
		ptr += pattern.size() + 1;
		filters.push_back({ label, pattern });
	}
	return filters;
}

std::string zenityFileFilter(const DialogFilter& filter)
{
	std::string pattern = filter.pattern;
	if (pattern == "*.*")
		pattern = "*"; // zenity matches names, not the extension dot
	return filter.label + " | " + pattern;
}

std::string kdialogFileFilter(const std::vector<DialogFilter>& filters)
{
	std::string spec;
	for (const DialogFilter& filter : filters) {
		if (!spec.empty())
			spec += "\n";
		std::string pattern = filter.pattern;
		if (pattern == "*.*")
			pattern = "*";
		spec += pattern + "|" + filter.label;
	}
	return spec;
}

std::string trimDialogResult(std::string text)
{
	while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
		text.pop_back();
	return text;
}

Uint32 sdlMsgBoxFlags(GuiUtils::MsgBoxIcon icon)
{
	switch (icon) {
	case GuiUtils::MsgBoxIcon::Information: return SDL_MESSAGEBOX_INFORMATION;
	case GuiUtils::MsgBoxIcon::Warning: return SDL_MESSAGEBOX_WARNING;
	case GuiUtils::MsgBoxIcon::Error: return SDL_MESSAGEBOX_ERROR;
	case GuiUtils::MsgBoxIcon::Empty: break;
	}
	return 0;
}

} // namespace

void GuiUtils::MsgBox_Ok(Window* window, const char* message, GuiUtils::MsgBoxIcon flags)
{
	if (SDL_ShowSimpleMessageBox(sdlMsgBoxFlags(flags), "XXL Editor", message, window ? window->getSDLWindow() : nullptr) == 0)
		return;
	// SDL has no dialog backend on this machine: use the desktop's own dialog program, and always keep
	// a copy of the message on the console.
	if (xxl::commandExists("zenity")) {
		std::string option = (flags == MsgBoxIcon::Error) ? "--error" : (flags == MsgBoxIcon::Warning ? "--warning" : "--info");
		if (xxl::runCommand({ "zenity", option, "--title=XXL Editor", std::string("--text=") + message }, nullptr) == 0)
			return;
	}
	else if (xxl::commandExists("kdialog")) {
		std::string option = (flags == MsgBoxIcon::Error) ? "--error" : (flags == MsgBoxIcon::Warning ? "--sorry" : "--msgbox");
		if (xxl::runCommand({ "kdialog", option, message, "--title", "XXL Editor" }, nullptr) == 0)
			return;
	}
	fprintf(stderr, "XXL Editor: %s\n", message);
}

GuiUtils::MsgBoxButton GuiUtils::MsgBox_YesNo(Window* window, const char* message, GuiUtils::MsgBoxIcon flags)
{
	const SDL_MessageBoxButtonData buttons[] = {
		{ SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, (int)MsgBoxButton::Yes, "Yes" },
		{ SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, (int)MsgBoxButton::No, "No" },
	};
	SDL_MessageBoxData data = {};
	data.flags = sdlMsgBoxFlags(flags);
	data.window = window ? window->getSDLWindow() : nullptr;
	data.title = "XXL Editor";
	data.message = message;
	data.numbuttons = 2;
	data.buttons = buttons;
	int buttonId = -1;
	if (SDL_ShowMessageBox(&data, &buttonId) == 0 && (buttonId == (int)MsgBoxButton::Yes || buttonId == (int)MsgBoxButton::No))
		return (MsgBoxButton)buttonId;
	if (xxl::commandExists("zenity")) {
		int result = xxl::runCommand({ "zenity", "--question", "--title=XXL Editor", std::string("--text=") + message, "--ok-label=Yes", "--cancel-label=No" }, nullptr);
		if (result == 0) return MsgBoxButton::Yes;
		if (result == 1) return MsgBoxButton::No;
	}
	else if (xxl::commandExists("kdialog")) {
		int result = xxl::runCommand({ "kdialog", "--warningyesno", message, "--title", "XXL Editor", "--yes-label", "Yes", "--no-label", "No" }, nullptr);
		if (result == 0) return MsgBoxButton::Yes;
		if (result == 1) return MsgBoxButton::No;
	}
	fprintf(stderr, "XXL Editor: %s\n", message);
	return MsgBoxButton::Unknown;
}

std::filesystem::path GuiUtils::OpenDialogBox(Window* window, const char* filter, const char* defExt)
{
	(void)defExt; // the desktop dialogs derive the extension from the chosen filter
	std::vector<DialogFilter> filters = parseFilterSpec(filter);
	std::string output;
	if (xxl::commandExists("zenity")) {
		std::vector<std::string> args = { "zenity", "--file-selection", "--title=XXL Editor" };
		for (const DialogFilter& entry : filters)
			args.push_back("--file-filter=" + zenityFileFilter(entry));
		if (xxl::runCommand(args, &output) == 0) {
			std::string path = trimDialogResult(std::move(output));
			return path.empty() ? std::filesystem::path{} : std::filesystem::path(path);
		}
		return {};
	}
	if (xxl::commandExists("kdialog")) {
		std::vector<std::string> args = { "kdialog", "--getopenfilename", ".", kdialogFileFilter(filters) };
		if (xxl::runCommand(args, &output) == 0) {
			std::string path = trimDialogResult(std::move(output));
			return path.empty() ? std::filesystem::path{} : std::filesystem::path(path);
		}
		return {};
	}
	MsgBox_Ok(window, "No file picker is available on this system.\nInstall zenity (GNOME) or kdialog (KDE) to choose files.", MsgBoxIcon::Warning);
	return {};
}

std::vector<std::filesystem::path> GuiUtils::MultiOpenDialogBox(Window* window, const char* filter, const char* defExt)
{
	(void)defExt;
	std::vector<DialogFilter> filters = parseFilterSpec(filter);
	std::vector<std::filesystem::path> list;
	std::string output;
	if (xxl::commandExists("zenity")) {
		std::vector<std::string> args = { "zenity", "--file-selection", "--multiple", "--separator=\n", "--title=XXL Editor" };
		for (const DialogFilter& entry : filters)
			args.push_back("--file-filter=" + zenityFileFilter(entry));
		if (xxl::runCommand(args, &output) != 0)
			return list;
	}
	else if (xxl::commandExists("kdialog")) {
		if (xxl::runCommand({ "kdialog", "--getopenfilename", ".", kdialogFileFilter(filters), "--multiple", "--separate-output" }, &output) != 0)
			return list;
	}
	else {
		MsgBox_Ok(window, "No file picker is available on this system.\nInstall zenity (GNOME) or kdialog (KDE) to choose files.", MsgBoxIcon::Warning);
		return list;
	}
	size_t start = 0;
	while (start <= output.size()) {
		size_t end = output.find('\n', start);
		std::string path = trimDialogResult(output.substr(start, (end == std::string::npos) ? std::string::npos : end - start));
		if (!path.empty())
			list.emplace_back(path);
		if (end == std::string::npos)
			break;
		start = end + 1;
	}
	return list;
}

std::filesystem::path GuiUtils::SaveDialogBox(Window* window, const char* filter, const char* defExt, const std::filesystem::path& defName)
{
	(void)defExt;
	std::vector<DialogFilter> filters = parseFilterSpec(filter);
	std::string output;
	if (xxl::commandExists("zenity")) {
		std::vector<std::string> args = { "zenity", "--file-selection", "--save", "--confirm-overwrite", "--title=XXL Editor" };
		for (const DialogFilter& entry : filters)
			args.push_back("--file-filter=" + zenityFileFilter(entry));
		if (!defName.empty())
			args.push_back("--filename=" + defName.filename().string());
		if (xxl::runCommand(args, &output) == 0) {
			std::string path = trimDialogResult(std::move(output));
			return path.empty() ? std::filesystem::path{} : std::filesystem::path(path);
		}
		return {};
	}
	if (xxl::commandExists("kdialog")) {
		std::vector<std::string> args = { "kdialog", "--getsavefilename", defName.empty() ? "." : defName.string(), kdialogFileFilter(filters) };
		if (xxl::runCommand(args, &output) == 0) {
			std::string path = trimDialogResult(std::move(output));
			return path.empty() ? std::filesystem::path{} : std::filesystem::path(path);
		}
		return {};
	}
	MsgBox_Ok(window, "No file picker is available on this system.\nInstall zenity (GNOME) or kdialog (KDE) to save files.", MsgBoxIcon::Warning);
	return {};
}

std::filesystem::path GuiUtils::SelectFolderDialogBox(Window* window, const char* text)
{
	std::string title = text ? text : "Select a folder";
	std::string output;
	if (xxl::commandExists("zenity")) {
		if (xxl::runCommand({ "zenity", "--file-selection", "--directory", "--title=" + title }, &output) == 0) {
			std::string path = trimDialogResult(std::move(output));
			return path.empty() ? std::filesystem::path{} : std::filesystem::path(path);
		}
		return {};
	}
	if (xxl::commandExists("kdialog")) {
		if (xxl::runCommand({ "kdialog", "--getexistingdirectory", ".", "--title", title }, &output) == 0) {
			std::string path = trimDialogResult(std::move(output));
			return path.empty() ? std::filesystem::path{} : std::filesystem::path(path);
		}
		return {};
	}
	// SDL2 has no folder picker of its own.
	MsgBox_Ok(window, "No folder dialog is available on this system.\nInstall zenity or kdialog (or the desktop portal) to pick folders.", MsgBoxIcon::Warning);
	return {};
}

// ---------------------------------------------------------------- text encodings ---
// The games store their text in Windows-1252, which MultiByteToWideChar decoded on Windows. The same
// conversion is done here with the 0x80..0x9F part of the code page spelled out.

namespace {

const uint16_t cp1252Special[32] = {
	0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
	0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
	0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
	0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178
};

uint32_t cp1252ToCodepoint(uint8_t byte)
{
	if (byte >= 0x80 && byte <= 0x9F)
		return cp1252Special[byte - 0x80];
	return byte;
}

uint8_t codepointToCp1252(uint32_t code)
{
	if (code < 0x80 || (code >= 0xA0 && code <= 0xFF))
		return (uint8_t)code;
	for (int i = 0; i < 32; i++)
		if (cp1252Special[i] == code)
			return (uint8_t)(0x80 + i);
	return (uint8_t)'?'; // unmappable characters become '?', as WideCharToMultiByte was told to do
}

void appendUtf8(std::string& out, uint32_t code)
{
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

std::string cp1252ToUtf8(std::string_view text)
{
	std::string out;
	out.reserve(text.size());
	for (char character : text)
		appendUtf8(out, cp1252ToCodepoint((uint8_t)character));
	return out;
}

uint32_t nextUtf8Codepoint(std::string_view text, size_t& index)
{
	uint8_t byte = (uint8_t)text[index];
	if (byte < 0x80) {
		index += 1;
		return byte;
	}
	size_t length;
	uint32_t code;
	if ((byte & 0xE0) == 0xC0) { length = 2; code = byte & 0x1F; }
	else if ((byte & 0xF0) == 0xE0) { length = 3; code = byte & 0x0F; }
	else if ((byte & 0xF8) == 0xF0) { length = 4; code = byte & 0x07; }
	else { index += 1; return 0xFFFD; }
	if (index + length > text.size()) {
		index = text.size();
		return 0xFFFD;
	}
	for (size_t i = 1; i < length; i++)
		code = (code << 6) | ((uint8_t)text[index + i] & 0x3F);
	index += length;
	return code;
}

std::string utf8ToCp1252(std::string_view text)
{
	std::string out;
	out.reserve(text.size());
	size_t index = 0;
	while (index < text.size())
		out.push_back((char)codepointToCp1252(nextUtf8Codepoint(text, index)));
	return out;
}

} // namespace

std::string GuiUtils::latinToUtf8(std::string_view text)
{
	return cp1252ToUtf8(text);
}

std::string GuiUtils::utf8ToLatin(std::string_view text)
{
	return utf8ToCp1252(text);
}

std::string GuiUtils::wcharToUtf8(std::wstring_view text)
{
	return xxl::narrowFromWide(text);
}

std::wstring GuiUtils::utf8ToWchar(std::string_view text)
{
	return xxl::widenFromUtf8(text);
}

errno_t GuiUtils::fsfopen_s(FILE** streamptr, const std::filesystem::path& filename, const char* mode)
{
	return fopen_s(streamptr, filename.string().c_str(), mode);
}

#endif
