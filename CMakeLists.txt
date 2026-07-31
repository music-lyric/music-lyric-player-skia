cmake_minimum_required(VERSION 3.21)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION.txt" MUSIC_LYRIC_PLAYER_VERSION LIMIT_COUNT 1)
string(STRIP "${MUSIC_LYRIC_PLAYER_VERSION}" MUSIC_LYRIC_PLAYER_VERSION)

project(music_lyric_player_skia VERSION ${MUSIC_LYRIC_PLAYER_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The prebuilt third-party libraries exist only as dynamic release, so every configuration has to link against that same CRT.
# Debug is dropped rather than kept working: it would pair a debug CRT with release-only prebuilts, and RelWithDebInfo already gives a debuggable build that links.
if(MSVC)
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL" CACHE STRING "" FORCE)

	if(CMAKE_CONFIGURATION_TYPES)
		set(CMAKE_CONFIGURATION_TYPES "Release;RelWithDebInfo;MinSizeRel" CACHE STRING "" FORCE)
		set(CMAKE_DEFAULT_BUILD_TYPE "Release" CACHE STRING "" FORCE)
	elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
		message(FATAL_ERROR "[Build] Debug is unsupported: the prebuilt Skia and lyric libraries are release-only; build Release or RelWithDebInfo instead")
	endif()
endif()

# Emscripten catches nothing by default, which turns any throw into a module abort rather than an error the caller can see.
# The prebuilt lyric model is compiled the same way, and the web link opts into the same handling.
if(EMSCRIPTEN)
	add_compile_options(-fwasm-exceptions)
endif()

# Prebuilt third-party libs; build them once with `cmake -P cmake/third-party/build.cmake`.
include(cmake/third-party/include.cmake)

add_subdirectory(src)

add_subdirectory(platform)

option(MUSIC_LYRIC_PLAYER_BUILD_EXAMPLE "Build Windows Example" OFF)
if(MUSIC_LYRIC_PLAYER_BUILD_EXAMPLE)
	add_subdirectory(example/windows)
endif()
