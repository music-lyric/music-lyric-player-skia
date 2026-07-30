cmake_minimum_required(VERSION 3.21)

if(NOT CMAKE_SCRIPT_MODE_FILE)
	message(FATAL_ERROR "[Build] Must be run in script mode")
endif()

if(NOT CMAKE_HOST_WIN32)
	message(FATAL_ERROR "[Build] The Windows module targets a Windows host")
endif()

get_filename_component(_repo_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

if(NOT DEFINED BUILD_CONFIG OR BUILD_CONFIG STREQUAL "")
	set(BUILD_CONFIG "Release")
endif()

# The module has no preset, so the build tree the former default preset described is spelled out here.
set(_cache_dir "${_repo_root}/out/app/cache/windows")

message(STATUS "[Build] Target: windows (${BUILD_CONFIG})")
message(STATUS "[Build] Cache : ${_cache_dir}")

message(STATUS "[Build] Configuring...")
execute_process(
	COMMAND "${CMAKE_COMMAND}" -S "${_repo_root}" -B "${_cache_dir}" -G "Ninja Multi-Config"
	WORKING_DIRECTORY "${_repo_root}"
	RESULT_VARIABLE _configure_rc)
if(NOT _configure_rc EQUAL 0)
	message(FATAL_ERROR "[Build] Configure failed (rc=${_configure_rc})")
endif()

message(STATUS "[Build] Building music_lyric_player_native...")
execute_process(
	COMMAND "${CMAKE_COMMAND}" --build "${_cache_dir}" --config "${BUILD_CONFIG}" --target music_lyric_player_native
	WORKING_DIRECTORY "${_repo_root}"
	RESULT_VARIABLE _build_rc)
if(NOT _build_rc EQUAL 0)
	message(FATAL_ERROR "[Build] Build failed (rc=${_build_rc})")
endif()

message(STATUS "[Build] Done")
