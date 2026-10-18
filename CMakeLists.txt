# ----------------------------------------------------------------------------------------------------
#  XXL Editor -- CMake build
#
#  This is the native Linux build: SDL2 for windowing/input, OpenGL for the viewport, native file
#  dialogs (zenity/kdialog or SDL's own dialogs) and POSIX process handling for "test level in game".
#  The MSVC solution (XXL-Editor.sln) stays the build for Windows; this file builds the same editor
#  there too, where it selects the D3D9 renderer.
#
#  Linux dependencies: SDL2, zlib, fmt, inih (INIReader), OpenGL. nlohmann/json, stb and libsquish are
#  used from the system when installed, otherwise from the bundled copies in third_party/.
# ----------------------------------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.16)
project(XXLEditor LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(XXL_THIRD_PARTY "${CMAKE_CURRENT_SOURCE_DIR}/third_party" CACHE PATH "Bundled fallback copies of nlohmann/json, stb and libsquish")
set(XXL_EXTRA_INCLUDE_DIRS "")

# ---------------------------------------------------------------- sources ---

set(XXL_SOURCE_DIRS
	"${CMAKE_CURRENT_SOURCE_DIR}"
	"${CMAKE_CURRENT_SOURCE_DIR}/CoreClasses"
	"${CMAKE_CURRENT_SOURCE_DIR}/GameClasses"
	"${CMAKE_CURRENT_SOURCE_DIR}/EditorUI"
	"${CMAKE_CURRENT_SOURCE_DIR}/imgui"
)
set(XXL_SOURCES "")
foreach(xxl_dir IN LISTS XXL_SOURCE_DIRS)
	file(GLOB xxl_dir_sources CONFIGURE_DEPENDS "${xxl_dir}/*.cpp")
	list(APPEND XXL_SOURCES ${xxl_dir_sources})
endforeach()

if(NOT WIN32)
	list(REMOVE_ITEM XXL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/renderer_d3d9.cpp")
endif()

# adpcm-xq is a subtree of this repository, compiled in as a static library module (as adpcm-xq.vcxproj does).
list(APPEND XXL_SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/adpcm-xq/adpcm-lib.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/adpcm-xq/adpcm-dns.c"
)

# ---------------------------------------------------------------- dependencies ---

find_package(ZLIB REQUIRED)

if(WIN32)
	find_package(SDL2 REQUIRED)
	find_package(fmt CONFIG REQUIRED)
	find_path(XXL_INIH_INCLUDE_DIR NAMES INIReader.h REQUIRED)
	find_package(nlohmann_json CONFIG REQUIRED)
else()
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
	pkg_check_modules(INIH REQUIRED IMPORTED_TARGET inih)
	pkg_check_modules(INI_READER QUIET IMPORTED_TARGET INIReader)
	if(NOT INI_READER_FOUND)
		# Some installs ship the C part of inih only; the INIReader C++ wrapper is then taken from the
		# bundled copy (same upstream project, same three BSD-3/MIT files).
		list(APPEND XXL_SOURCES "${XXL_THIRD_PARTY}/inih/INIReader.cpp" "${XXL_THIRD_PARTY}/inih/ini.c")
		list(APPEND XXL_EXTRA_INCLUDE_DIRS "${XXL_THIRD_PARTY}/inih")
	endif()
	find_package(fmt QUIET)
	if(NOT fmt_FOUND)
		pkg_check_modules(FMT REQUIRED IMPORTED_TARGET fmt)
	endif()
endif()

# nlohmann/json -- header only, not always packaged
find_package(nlohmann_json QUIET)
if(NOT TARGET nlohmann_json::nlohmann_json)
	set(XXL_JSON_INCLUDE_DIR "${XXL_THIRD_PARTY}/json")
	if(NOT EXISTS "${XXL_JSON_INCLUDE_DIR}/nlohmann/json.hpp")
		message(FATAL_ERROR "nlohmann/json was not found. Install it (e.g. nlohmann-json3-dev / nlohmann-json) or vendor the single header at ${XXL_JSON_INCLUDE_DIR}/nlohmann/json.hpp")
	endif()
endif()

# stb -- header only
find_path(XXL_STB_INCLUDE_DIR NAMES stb_image.h)
if(NOT XXL_STB_INCLUDE_DIR)
	set(XXL_STB_INCLUDE_DIR "${XXL_THIRD_PARTY}/stb")
	if(NOT EXISTS "${XXL_STB_INCLUDE_DIR}/stb_image.h")
		message(FATAL_ERROR "stb was not found. Install it (e.g. libstb-dev / stb) or vendor stb_image.h and stb_image_write.h in ${XXL_STB_INCLUDE_DIR}")
	endif()
endif()

# libsquish -- used for DXT1/DXT5 texture compression; rarely packaged, so the bundled copy is built when absent
find_path(XXL_SQUISH_INCLUDE_DIR NAMES squish.h)
find_library(XXL_SQUISH_LIBRARY NAMES squish)
if(NOT XXL_SQUISH_INCLUDE_DIR OR NOT XXL_SQUISH_LIBRARY)
	set(XXL_SQUISH_INCLUDE_DIR "${XXL_THIRD_PARTY}/squish")
	file(GLOB XXL_SQUISH_SOURCES CONFIGURE_DEPENDS "${XXL_SQUISH_INCLUDE_DIR}/*.cpp")
	if(NOT XXL_SQUISH_SOURCES)
		message(FATAL_ERROR "libsquish was not found. Install it (libsquish-dev) or vendor its sources in ${XXL_SQUISH_INCLUDE_DIR}")
	endif()
endif()

# ---------------------------------------------------------------- target ---

add_executable(xxl-editor ${XXL_SOURCES} ${XXL_SQUISH_SOURCES})

target_include_directories(xxl-editor PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}"
	"${CMAKE_CURRENT_SOURCE_DIR}/imgui"
	${XXL_STB_INCLUDE_DIR}
	${XXL_SQUISH_INCLUDE_DIR}
	${XXL_EXTRA_INCLUDE_DIRS}
)
if(XXL_JSON_INCLUDE_DIR)
	target_include_directories(xxl-editor PRIVATE ${XXL_JSON_INCLUDE_DIR})
else()
	target_link_libraries(xxl-editor PRIVATE nlohmann_json::nlohmann_json)
endif()

target_link_libraries(xxl-editor PRIVATE ZLIB::ZLIB)
if(XXL_SQUISH_LIBRARY)
	target_link_libraries(xxl-editor PRIVATE ${XXL_SQUISH_LIBRARY})
endif()

if(WIN32)
	target_link_libraries(xxl-editor PRIVATE SDL2::SDL2 fmt::fmt)
	target_include_directories(xxl-editor PRIVATE ${XXL_INIH_INCLUDE_DIR})
	target_link_libraries(xxl-editor PRIVATE d3d9 opengl32)
else()
	target_link_libraries(xxl-editor PRIVATE PkgConfig::SDL2 PkgConfig::INIH)
	if(INI_READER_FOUND)
		target_link_libraries(xxl-editor PRIVATE PkgConfig::INI_READER)
	endif()
	if(fmt_FOUND)
		target_link_libraries(xxl-editor PRIVATE fmt::fmt)
	else()
		target_link_libraries(xxl-editor PRIVATE PkgConfig::FMT)
	endif()
	find_package(OpenGL REQUIRED)
	target_link_libraries(xxl-editor PRIVATE OpenGL::GL m dl pthread)
endif()

# Copy the data resources (fonts, encyclopedia JSON, toolbar icons, ...) next to the binary, so the
# editor can be started from the build directory. GetResourceContent also looks in ./resources and
# ./xec_resources, which is where a modder would override them.
add_custom_command(TARGET xxl-editor POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CMAKE_CURRENT_SOURCE_DIR}/resources"
		"$<TARGET_FILE_DIR:xxl-editor>/resources"
	COMMENT "Copying the editor's resources next to the binary"
)

if(NOT WIN32)
	# MSVC leaves char8_t disabled in C++20 mode, so path::u8string() stays a std::string there.
	# GCC and Clang enable char8_t for -std=c++20; -fno-char8_t keeps the two compilers behaving the
	# same on the u8string() call sites instead of rewriting all of them.
	target_compile_options(xxl-editor PRIVATE -fno-char8_t)
endif()

install(TARGETS xxl-editor RUNTIME DESTINATION bin)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/resources" DESTINATION share/xxl-editor)
