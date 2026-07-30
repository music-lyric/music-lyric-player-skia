cmake_minimum_required(VERSION 3.21)

if(NOT CMAKE_SCRIPT_MODE_FILE)
	message(FATAL_ERROR "[Package] Must be run in script mode")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../common/platform.cmake")

get_filename_component(_repo_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

# The distribution ships Release only, which is the sole configuration the web module is built in.
if(NOT DEFINED PACKAGE_CONFIG)
	set(PACKAGE_CONFIG "Release")
endif()
if(NOT DEFINED PACKAGE_PLATFORM)
	set(PACKAGE_PLATFORM "web")
endif()
if(NOT DEFINED PACKAGE_ARCH)
	set(PACKAGE_ARCH "wasm32")
endif()

# Resolve the canonical triple with the shared helper so the paths match what platform/CMakeLists.txt wrote.
platform_resolve_target(
	"${PACKAGE_PLATFORM}" "${PACKAGE_ARCH}" "${PACKAGE_CONFIG}"
	_platform _arch _cfg)

if(NOT _platform STREQUAL "web")
	message(FATAL_ERROR "[Package] This script packages Web only, got: ${_platform}")
endif()
if(NOT _cfg STREQUAL "release")
	message(FATAL_ERROR "[Package] Only Release is packaged, got: ${PACKAGE_CONFIG}")
endif()

set(_triple "${_platform}-${_arch}-${_cfg}")

set(_build_dir "${_repo_root}/out/app/build/${_triple}")

# The npm publish root is the package directory itself, so the payload lands in its ignored dist/ rather than a staging tree.
set(_package_dir "${_repo_root}/platform/web")
set(_package_dist "${_package_dir}/dist")

set(_dist_dir "${_repo_root}/out/app/dist")
set(_stage_dir "${_dist_dir}/${_triple}")
set(_zip "${_dist_dir}/${_triple}.zip")

message(STATUS "[Package] Target : ${_triple}")
message(STATUS "[Package] Build  : ${_build_dir}")
message(STATUS "[Package] Package: ${_package_dist}")
message(STATUS "[Package] Output : ${_zip}")

# Build the module the same way the project does, unless it is already built.
# The build script owns how the module is configured, so packaging and `make web-build` cannot drift apart.
if(NOT PACKAGE_SKIP_BUILD)
	set(BUILD_CONFIG "${PACKAGE_CONFIG}")
	include("${CMAKE_CURRENT_LIST_DIR}/../build/web.cmake")
endif()

# The glue, the module, and the embind declarations all have to exist before anything is copied.
foreach(_artifact "music_lyric_player.mjs" "music_lyric_player.wasm" "music_lyric_player.d.ts")
	if(NOT EXISTS "${_build_dir}/${_artifact}")
		message(FATAL_ERROR "[Package] Missing build artifact: ${_build_dir}/${_artifact}")
	endif()
endforeach()

# Replace dist/ wholesale so a renamed or dropped artifact cannot survive as a stale file in the published package.
message(STATUS "[Package] Publishing into the package...")
file(REMOVE_RECURSE "${_package_dist}")
file(MAKE_DIRECTORY "${_package_dist}")
file(GLOB _artifacts "${_build_dir}/*")
file(COPY ${_artifacts} DESTINATION "${_package_dist}")

set(_payload
	"dist"
	"types"
	"index.js"
	"index.d.ts"
	"package.json"
	"README.md"
	"LICENSE")

message(STATUS "[Package] Staging...")
file(REMOVE_RECURSE "${_stage_dir}")
file(MAKE_DIRECTORY "${_stage_dir}")

foreach(_entry IN LISTS _payload)
	if(NOT EXISTS "${_package_dir}/${_entry}")
		message(FATAL_ERROR "[Package] Missing package entry: ${_package_dir}/${_entry}")
	endif()
	file(COPY "${_package_dir}/${_entry}" DESTINATION "${_stage_dir}")
endforeach()

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
