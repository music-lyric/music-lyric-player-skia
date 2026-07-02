# Include lyric model

include_guard(GLOBAL)

set(LYRIC_MODEL_SRC "${CMAKE_SOURCE_DIR}/third-party/lyric-model" CACHE PATH)

if(NOT EXISTS "${LYRIC_MODEL_SRC}/CMakeLists.txt")
	message(FATAL_ERROR
		"lyric-model submodule not found at: ${LYRIC_MODEL_SRC}\n"
		"    git submodule update --init third-party/lyric-model")
endif()

add_subdirectory("${LYRIC_MODEL_SRC}" "${CMAKE_BINARY_DIR}/third-party/lyric-model")

if(NOT TARGET music_lyric_model::music_lyric_model)
	message(FATAL_ERROR
		"Target music_lyric_model::music_lyric_model not found after add_subdirectory.\n"
		"The submodule may be at an incompatible revision.")
endif()
