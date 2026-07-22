cmake_minimum_required(VERSION 3.21)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION.txt" MUSIC_LYRIC_PLAYER_VERSION LIMIT_COUNT 1)
string(STRIP "${MUSIC_LYRIC_PLAYER_VERSION}" MUSIC_LYRIC_PLAYER_VERSION)

project(music_lyric_player_skia VERSION ${MUSIC_LYRIC_PLAYER_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Match the MSVC runtime with the prebuilt third-party libraries (/MD for Release, /MDd for Debug).
if(MSVC)
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" CACHE STRING "" FORCE)
endif()

# Prebuilt third-party libs; build them once with `cmake -P cmake/third-party/build.cmake`.
include(cmake/third-party/include.cmake)

add_subdirectory(src)

option(MUSIC_LYRIC_PLAYER_BUILD_EXAMPLE "Build Windows Example" OFF)
if(MUSIC_LYRIC_PLAYER_BUILD_EXAMPLE)
	add_subdirectory(example/windows)
endif()
