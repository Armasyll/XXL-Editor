Vendored third-party code
=========================

These are fallback copies: CMake uses the library from the system whenever it is installed, and only
builds what it finds here when the system does not provide it. They are unchanged upstream sources.

* `json/` -- nlohmann/json v3.11.3 single header (`nlohmann/json.hpp`, `nlohmann/json_fwd.hpp`), MIT.
  https://github.com/nlohmann/json -- used when `find_package(nlohmann_json)` finds nothing.
* `stb/` -- `stb_image.h` and `stb_image_write.h` from https://github.com/nothings/stb (master,
  fetched 2026-10-18), public domain / MIT. Used when the system has no stb headers.
* `squish/` -- libsquish 1.15 sources (DXT1/DXT3/DXT5 compression), MIT, as mirrored at
  https://github.com/akb825/libsquish. Used when `squish.h` or `libsquish` are not installed.
* `inih/` -- `INIReader.cpp`, `ini.c`, `ini.h` from https://github.com/benhoyt/inih, BSD-3-Clause.
  Only used when inih is installed without the INIReader C++ wrapper (`pkg-config INIReader` missing).

Each directory keeps its upstream license text where the project ships one (`squish/LICENSE.txt`).
