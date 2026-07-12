cmake_minimum_required(VERSION 3.21)

project(music_lyric_player_skia VERSION 0.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Skia and the prebuilt lyric model ship with the dynamic release runtime; force /MD so the app links against both.
# Debug uses /MDd, so build as Release or RelWithDebInfo unless matching debug deps exist.
if(MSVC)
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL" CACHE STRING "" FORCE)
endif()

# Prebuilt third-party libs; build each once with `cmake -P cmake/<lib>/build.cmake`.
include(cmake/lyric/include.cmake)
include(cmake/skia/include.cmake)

add_subdirectory(src)

option(MUSIC_LYRIC_PLAYER_BUILD_EXAMPLE "Build Windows Example" OFF)
if(MUSIC_LYRIC_PLAYER_BUILD_EXAMPLE)
	add_subdirectory(example/windows)
endif()
