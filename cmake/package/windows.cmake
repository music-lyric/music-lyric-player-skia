cmake_minimum_required(VERSION 3.21)

if(NOT CMAKE_SCRIPT_MODE_FILE)
	message(FATAL_ERROR "[Package] Must be run in script mode")
endif()

if(NOT CMAKE_HOST_WIN32)
	message(FATAL_ERROR "[Package] The Windows package targets a Windows host")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../common/platform.cmake")

get_filename_component(_repo_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

# The distribution ships Release only, since Debug needs the debug CRT that the prebuilt Skia lacks.
if(NOT DEFINED PACKAGE_CONFIG)
	set(PACKAGE_CONFIG "Release")
endif()
if(NOT DEFINED PACKAGE_PLATFORM)
	set(PACKAGE_PLATFORM "windows")
endif()
if(NOT DEFINED PACKAGE_ARCH)
	set(PACKAGE_ARCH "${CMAKE_HOST_SYSTEM_PROCESSOR}")
	if(PACKAGE_ARCH STREQUAL "")
		set(PACKAGE_ARCH "x64")
	endif()
endif()

# Resolve the canonical triple with the shared helper so the paths match what src/CMakeLists.txt wrote.
platform_resolve_target(
	"${PACKAGE_PLATFORM}" "${PACKAGE_ARCH}" "${PACKAGE_CONFIG}"
	_platform _arch _cfg)

if(NOT _platform STREQUAL "windows")
	message(FATAL_ERROR "[Package] This script packages Windows only, got: ${_platform}")
endif()
if(NOT _cfg STREQUAL "release")
	message(FATAL_ERROR "[Package] Only Release is packaged (Debug is unsupported), got: ${PACKAGE_CONFIG}")
endif()

set(_triple "${_platform}-${_arch}-${_cfg}")

set(_build_dir "${_repo_root}/out/app/build/${_triple}")
set(_dll "${_build_dir}/music_lyric_player_native.dll")
set(_lib "${_build_dir}/music_lyric_player_native.lib")
set(_include_src "${_repo_root}/include")

set(_dist_dir "${_repo_root}/out/app/dist")
set(_stage_dir "${_dist_dir}/${_triple}")
set(_zip "${_dist_dir}/${_triple}.zip")

message(STATUS "[Package] Target : ${_triple}")
message(STATUS "[Package] Build  : ${_build_dir}")
message(STATUS "[Package] Output : ${_zip}")

# Build the shared library through the same presets the project uses, unless it is already built.
if(NOT PACKAGE_SKIP_BUILD)
	message(STATUS "[Package] Configuring...")
	execute_process(
		COMMAND "${CMAKE_COMMAND}" --preset default
		WORKING_DIRECTORY "${_repo_root}"
		RESULT_VARIABLE _configure_rc)
	if(NOT _configure_rc EQUAL 0)
		message(FATAL_ERROR "[Package] Configure failed (rc=${_configure_rc})")
	endif()

	message(STATUS "[Package] Building music_lyric_player_native...")
	execute_process(
		COMMAND "${CMAKE_COMMAND}" --build --preset release --target music_lyric_player_native
		WORKING_DIRECTORY "${_repo_root}"
		RESULT_VARIABLE _build_rc)
	if(NOT _build_rc EQUAL 0)
		message(FATAL_ERROR "[Package] Build failed (rc=${_build_rc})")
	endif()
endif()

# Both the DLL and its import library must exist before staging; ICU is embedded, so no icudtl.dat ships.
foreach(_artifact "${_dll}" "${_lib}")
	if(NOT EXISTS "${_artifact}")
		message(FATAL_ERROR "[Package] Missing build artifact: ${_artifact}")
	endif()
endforeach()

# Lay the payload out as bin/ lib/ include/ inside a staging folder that becomes the zip's root.
message(STATUS "[Package] Staging...")
file(REMOVE_RECURSE "${_stage_dir}")
file(MAKE_DIRECTORY
	"${_stage_dir}/bin"
	"${_stage_dir}/lib"
	"${_stage_dir}/include/music_lyric_player")

file(COPY "${_dll}" DESTINATION "${_stage_dir}/bin")
file(COPY "${_lib}" DESTINATION "${_stage_dir}/lib")

# The public headers cross-include by relative path, so they sit together under one prefixed directory.
file(GLOB _headers "${_include_src}/*.h")
if(_headers STREQUAL "")
	message(FATAL_ERROR "[Package] No public headers found in ${_include_src}")
endif()
file(COPY ${_headers} DESTINATION "${_stage_dir}/include/music_lyric_player")

message(STATUS "[Package] Archiving...")
file(REMOVE "${_zip}")
execute_process(
	COMMAND "${CMAKE_COMMAND}" -E tar cf "${_triple}.zip" --format=zip "${_triple}"
	WORKING_DIRECTORY "${_dist_dir}"
	RESULT_VARIABLE _archive_rc)
if(NOT _archive_rc EQUAL 0)
	message(FATAL_ERROR "[Package] Archive failed (rc=${_archive_rc})")
endif()

# Leave only the zip behind, since the staging tree was just scaffolding for the archive.
file(REMOVE_RECURSE "${_stage_dir}")

message(STATUS "[Package] Done: ${_zip}")
