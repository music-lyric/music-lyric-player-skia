cmake_minimum_required(VERSION 3.21)

if(NOT CMAKE_SCRIPT_MODE_FILE)
	message(FATAL_ERROR "[Build] Must be run in script mode")
endif()

get_filename_component(_repo_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

if(NOT DEFINED BUILD_CONFIG OR BUILD_CONFIG STREQUAL "")
	set(BUILD_CONFIG "Release")
endif()

# The module has no preset, so the build tree the former web preset described is spelled out here.
set(_cache_dir "${_repo_root}/out/app/cache/web")
set(_toolchain "${_repo_root}/cmake/tools/emscripten.cmake")

message(STATUS "[Build] Target: web (${BUILD_CONFIG})")
message(STATUS "[Build] Cache : ${_cache_dir}")

message(STATUS "[Build] Configuring...")
execute_process(
	COMMAND "${CMAKE_COMMAND}" -S "${_repo_root}" -B "${_cache_dir}" -G "Ninja"
		"-DCMAKE_TOOLCHAIN_FILE=${_toolchain}" "-DCMAKE_BUILD_TYPE=${BUILD_CONFIG}"
	WORKING_DIRECTORY "${_repo_root}"
	RESULT_VARIABLE _configure_rc)
if(NOT _configure_rc EQUAL 0)
	message(FATAL_ERROR "[Build] Configure failed (rc=${_configure_rc})")
endif()

# Emscripten drives a single-config generator, so the build type comes from the cache rather than --config.
message(STATUS "[Build] Building music_lyric_player_web...")
execute_process(
	COMMAND "${CMAKE_COMMAND}" --build "${_cache_dir}" --target music_lyric_player_web
	WORKING_DIRECTORY "${_repo_root}"
	RESULT_VARIABLE _build_rc)
if(NOT _build_rc EQUAL 0)
	message(FATAL_ERROR "[Build] Build failed (rc=${_build_rc})")
endif()

message(STATUS "[Build] Done")
