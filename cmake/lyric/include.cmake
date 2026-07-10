# Include lyric model

include_guard(GLOBAL)

set(LYRIC_SRC "${CMAKE_SOURCE_DIR}/third-party/lyric" CACHE PATH "")

if(NOT EXISTS "${LYRIC_SRC}/CMakeLists.txt")
	message(FATAL_ERROR "lyric submodule not found at: ${LYRIC_SRC}")
endif()

add_subdirectory("${LYRIC_SRC}" "${CMAKE_BINARY_DIR}/third-party/lyric")

if(NOT TARGET music_lyric::model)
	message(FATAL_ERROR
		"Target music_lyric::model not found after add_subdirectory.\n"
		"The submodule may be at an incompatible revision.")
endif()
