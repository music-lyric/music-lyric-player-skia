cmake_minimum_required(VERSION 3.21)

if(NOT CMAKE_SCRIPT_MODE_FILE)
	message(FATAL_ERROR "[Glaze] Must be run in script mode")
endif()

get_filename_component(_repo_root "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(GLAZE_SRC "${_repo_root}/third-party/glaze")

if(NOT EXISTS "${GLAZE_SRC}/include/glaze/glaze.hpp")
	message(FATAL_ERROR "[Glaze] Submodule not found: ${GLAZE_SRC}")
endif()

message(STATUS "[Glaze] Header only, nothing to build: ${GLAZE_SRC}/include")
