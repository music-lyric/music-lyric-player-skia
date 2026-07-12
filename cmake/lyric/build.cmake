# Standalone lyric model build. Run: cmake -P cmake/lyric/build.cmake
# Optional args (before -P): -D LYRIC_BUILD_CONFIG=Debug|Release / -D LYRIC_BUILD_CLEAN=ON / -D LYRIC_BUILD_JOBS=8

cmake_minimum_required(VERSION 3.21)

if(NOT CMAKE_SCRIPT_MODE_FILE)
	message(FATAL_ERROR "Run as a script: cmake -P cmake/lyric/build.cmake")
endif()

if(NOT DEFINED LYRIC_BUILD_CONFIG)
	set(LYRIC_BUILD_CONFIG "Release")
endif()

get_filename_component(_repo_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(LYRIC_SRC "${_repo_root}/third-party/lyric")
string(TOLOWER "${LYRIC_BUILD_CONFIG}" _cfg)
set(LYRIC_OUT "${LYRIC_SRC}/out/${_cfg}")

message(STATUS "[Lyric] Source : ${LYRIC_SRC}")
message(STATUS "[Lyric] Output : ${LYRIC_OUT}")
message(STATUS "[Lyric] Config : ${LYRIC_BUILD_CONFIG}")

if(NOT EXISTS "${LYRIC_SRC}/CMakeLists.txt")
	message(FATAL_ERROR
		"Lyric submodule not found at: ${LYRIC_SRC}\n"
		"    git submodule update --init third-party/lyric")
endif()

find_program(NINJA NAMES ninja)
if(NOT NINJA)
	message(FATAL_ERROR "ninja not found on PATH.")
endif()

if(LYRIC_BUILD_CLEAN AND EXISTS "${LYRIC_OUT}")
	message(STATUS "[Lyric] Cleaning: ${LYRIC_OUT}")
	file(REMOVE_RECURSE "${LYRIC_OUT}")
endif()

# Ninja single-config + /MD to match Skia and the app.
execute_process(
	COMMAND "${CMAKE_COMMAND}"
		-S "${LYRIC_SRC}"
		-B "${LYRIC_OUT}"
		-G Ninja
		"-DCMAKE_BUILD_TYPE=${LYRIC_BUILD_CONFIG}"
		"-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL"
		"-Dprotobuf_MSVC_STATIC_RUNTIME=OFF"
	RESULT_VARIABLE _cfg_rc)
if(NOT _cfg_rc EQUAL 0)
	message(FATAL_ERROR "cmake configure failed (rc=${_cfg_rc})")
endif()

set(_build_cmd "${CMAKE_COMMAND}" --build "${LYRIC_OUT}" --target music_lyric_model)
if(LYRIC_BUILD_JOBS)
	list(APPEND _build_cmd --parallel "${LYRIC_BUILD_JOBS}")
endif()
execute_process(COMMAND ${_build_cmd} RESULT_VARIABLE _build_rc)
if(NOT _build_rc EQUAL 0)
	message(FATAL_ERROR "cmake build failed (rc=${_build_rc})")
endif()

if(CMAKE_HOST_WIN32)
	set(_model_lib "${LYRIC_OUT}/music_lyric_model.lib")
else()
	set(_model_lib "${LYRIC_OUT}/libmusic_lyric_model.a")
endif()
if(NOT EXISTS "${_model_lib}")
	message(FATAL_ERROR "Product missing: ${_model_lib}")
endif()

message(STATUS "[Lyric] Done: ${_model_lib}")
