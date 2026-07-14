# Standalone script kept for parity with cmake/{skia,lyric}/build.cmake. Glaze is header-only, so there
# is nothing to compile; this only verifies the submodule is present. Run: cmake -P cmake/glaze/build.cmake

cmake_minimum_required(VERSION 3.21)

if(NOT CMAKE_SCRIPT_MODE_FILE)
	message(FATAL_ERROR
		"This is a standalone script. Run it with:\n"
		"    cmake -P cmake/glaze/build.cmake")
endif()

get_filename_component(_repo_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(GLAZE_SRC "${_repo_root}/third-party/glaze")

if(NOT EXISTS "${GLAZE_SRC}/include/glaze/glaze.hpp")
	message(FATAL_ERROR
		"[Glaze] submodule not found at: ${GLAZE_SRC}\n"
		"    Init it: git submodule update --init third-party/glaze")
endif()

message(STATUS "[Glaze] header-only, nothing to build; headers at ${GLAZE_SRC}/include")
