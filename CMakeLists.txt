cmake_minimum_required(VERSION 3.21)

project(music_lyric_player_skia VERSION 0.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Skia ships prebuilt with the dynamic release runtime (/MD) for every configuration.
# Force every consumer, including the lyric model's protobuf and abseil, onto /MD before any target is created.
# Protobuf otherwise links the static /MT runtime and fails to link against Skia.
# Debug still uses /MDd for these dependencies, so build the app as Release or RelWithDebInfo unless a matching debug Skia is built.
if(MSVC)
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL" CACHE STRING "" FORCE)
	set(protobuf_MSVC_STATIC_RUNTIME OFF CACHE BOOL "" FORCE)
endif()

# lyric
include(cmake/lyric/include.cmake)
# skia
include(cmake/skia/include.cmake)

add_subdirectory(src)

option(MUSIC_LYRIC_PLAYER_BUILD_EXAMPLE "Build Windows Example" OFF)
if(MUSIC_LYRIC_PLAYER_BUILD_EXAMPLE)
	add_subdirectory(example/windows)
endif()
