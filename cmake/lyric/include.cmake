# Import the prebuilt lyric model (see cmake/lyric/build.cmake) as target music_lyric::model.

include_guard(GLOBAL)

set(LYRIC_SRC "${CMAKE_SOURCE_DIR}/third-party/lyric" CACHE PATH "Lyric model submodule source dir")

if(NOT EXISTS "${LYRIC_SRC}/CMakeLists.txt")
	message(FATAL_ERROR "lyric submodule not found at: ${LYRIC_SRC}")
endif()

if(NOT DEFINED LYRIC_BUILD_CONFIG)
	set(LYRIC_BUILD_CONFIG "Release")
endif()

if(DEFINED LYRIC_OUT_DIR)
	set(LYRIC_OUT "${LYRIC_OUT_DIR}")
else()
	string(TOLOWER "${LYRIC_BUILD_CONFIG}" _cfg)
	set(LYRIC_OUT "${LYRIC_SRC}/out/${_cfg}")
endif()

set(_model_lib "${LYRIC_OUT}/${CMAKE_STATIC_LIBRARY_PREFIX}music_lyric_model${CMAKE_STATIC_LIBRARY_SUFFIX}")
if(NOT EXISTS "${_model_lib}")
	message(FATAL_ERROR
		"[Lyric] Prebuilt model not found: ${_model_lib}\n"
		"    Build it first: cmake -P cmake/lyric/build.cmake")
endif()

# Model plus its protobuf / abseil archives (~80 libs).
file(GLOB_RECURSE LYRIC_LIBRARIES "${LYRIC_OUT}/*${CMAKE_STATIC_LIBRARY_SUFFIX}")
if(NOT LYRIC_LIBRARIES)
	message(FATAL_ERROR "[Lyric] No static libraries found under: ${LYRIC_OUT}")
endif()

message(STATUS "[Lyric] ${LYRIC_OUT}")

add_library(music_lyric_model INTERFACE)
add_library(music_lyric::model ALIAS music_lyric_model)

target_include_directories(music_lyric_model SYSTEM INTERFACE
	"${LYRIC_SRC}/include"
	"${LYRIC_SRC}/gen"
	"${LYRIC_OUT}/_deps/protobuf-src/src"
	"${LYRIC_OUT}/_deps/protobuf-src/third_party/utf8_range"
	"${LYRIC_OUT}/_deps/absl-src")
target_link_libraries(music_lyric_model INTERFACE ${LYRIC_LIBRARIES})
target_compile_features(music_lyric_model INTERFACE cxx_std_17)
target_compile_options(music_lyric_model INTERFACE $<$<CXX_COMPILER_ID:MSVC>:/utf-8;/bigobj>)
