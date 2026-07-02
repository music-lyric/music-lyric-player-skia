cmake_minimum_required(VERSION 3.21)

project(music_lyric_player_skia VERSION 0.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# lyric
include(cmake/lyric/include.cmake)
# skia
include(cmake/skia/include.cmake)

add_subdirectory(src)
