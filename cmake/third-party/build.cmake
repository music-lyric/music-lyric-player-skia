cmake_minimum_required(VERSION 3.21)

if(NOT CMAKE_SCRIPT_MODE_FILE)
	message(FATAL_ERROR "[ThirdParty] Must be run in script mode")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/lyric/build.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/skia/build.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/glaze/build.cmake")
