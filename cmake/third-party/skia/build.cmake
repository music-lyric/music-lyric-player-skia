cmake_minimum_required(VERSION 3.21)

if(NOT CMAKE_SCRIPT_MODE_FILE)
	message(FATAL_ERROR "[Skia] Must be run in script mode")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../../common/android.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../../common/emsdk.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../../common/platform.cmake")

set(SKIA_MODULES skia skshaper skunicode_core skunicode_icu)

if(NOT DEFINED SKIA_BUILD_CONFIG)
	set(SKIA_BUILD_CONFIG "Release")
endif()

if(NOT DEFINED SKIA_BUILD_PLATFORM)
	if(CMAKE_HOST_WIN32)
		set(SKIA_BUILD_PLATFORM "windows")
	else()
		message(FATAL_ERROR "[Skia] SKIA_BUILD_PLATFORM is required when cross-compiling (windows, android, web)")
	endif()
endif()
string(TOLOWER "${SKIA_BUILD_PLATFORM}" _platform_input)

if(NOT DEFINED SKIA_BUILD_ARCH)
	if(_platform_input STREQUAL "windows")
		set(SKIA_BUILD_ARCH "${CMAKE_HOST_SYSTEM_PROCESSOR}")
		if(SKIA_BUILD_ARCH STREQUAL "" AND DEFINED ENV{PROCESSOR_ARCHITECTURE})
			set(SKIA_BUILD_ARCH "$ENV{PROCESSOR_ARCHITECTURE}")
		endif()
		if(SKIA_BUILD_ARCH STREQUAL "")
			set(SKIA_BUILD_ARCH "x64")
		endif()
	elseif(_platform_input STREQUAL "android")
		set(SKIA_BUILD_ARCH "arm64-v8a")
	else()
		set(SKIA_BUILD_ARCH "wasm32")
	endif()
endif()

platform_resolve_target(
	"${SKIA_BUILD_PLATFORM}" "${SKIA_BUILD_ARCH}" "${SKIA_BUILD_CONFIG}"
	_platform _arch _cfg)

if(_arch STREQUAL "arm64-v8a")
	set(_gn_cpu "arm64")
elseif(_arch STREQUAL "armeabi-v7a")
	set(_gn_cpu "arm")
elseif(_arch STREQUAL "x86_64")
	set(_gn_cpu "x64")
elseif(_arch STREQUAL "wasm32")
	set(_gn_cpu "wasm")
else()
	set(_gn_cpu "${_arch}")
endif()

get_filename_component(_repo_root "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(SKIA_SRC "${_repo_root}/third-party/skia")
set(SKIA_OUT "${SKIA_SRC}/out/${_platform}-${_arch}-${_cfg}")
set(SKIA_STAGE "${_repo_root}/out/third-party/skia/${_platform}-${_arch}-${_cfg}")
set(_gn_args_common "${CMAKE_CURRENT_LIST_DIR}/args/common.gn")
set(_gn_args_platform "${CMAKE_CURRENT_LIST_DIR}/args/${_platform}.gn")

message(STATUS "[Skia] Source   : ${SKIA_SRC}")
message(STATUS "[Skia] Output   : ${SKIA_OUT}")
message(STATUS "[Skia] Stage    : ${SKIA_STAGE}")
message(STATUS "[Skia] Platform : ${_platform}")
message(STATUS "[Skia] Arch     : ${_arch}")
message(STATUS "[Skia] Config   : ${SKIA_BUILD_CONFIG}")
message(STATUS "[Skia] Args     : ${_gn_args_platform}")

if(NOT EXISTS "${SKIA_SRC}/BUILD.gn")
	message(FATAL_ERROR "[Skia] Submodule not found: ${SKIA_SRC}")
endif()
if(NOT EXISTS "${_gn_args_platform}")
	message(FATAL_ERROR "[Skia] Args not found: ${_gn_args_platform}")
endif()

find_program(PYTHON NAMES python python3)
if(NOT PYTHON)
	message(FATAL_ERROR "[Skia] Python not found")
endif()

find_program(NINJA NAMES ninja)
if(NOT NINJA)
	foreach(_cand "${SKIA_SRC}/third_party/ninja/ninja.exe" "${SKIA_SRC}/third_party/ninja/ninja")
		if(EXISTS "${_cand}")
			set(NINJA "${_cand}")
			break()
		endif()
	endforeach()
endif()
if(NOT NINJA)
	message(FATAL_ERROR "[Skia] Ninja not found")
endif()

if(CMAKE_HOST_WIN32)
	set(_gn_bundled "${SKIA_SRC}/bin/gn.exe")
else()
	set(_gn_bundled "${SKIA_SRC}/bin/gn")
endif()

if(_platform STREQUAL "windows")
	if(NOT CMAKE_HOST_WIN32)
		message(FATAL_ERROR "[Skia] Windows build requires a Windows host")
	endif()
	if(NOT DEFINED SKIA_CLANG_WIN)
		find_program(_CLANG_CL NAMES clang-cl)
		if(NOT _CLANG_CL)
			message(FATAL_ERROR "[Skia] Clang-cl not found")
		endif()
		get_filename_component(_bin_dir "${_CLANG_CL}" DIRECTORY)
		get_filename_component(SKIA_CLANG_WIN "${_bin_dir}" DIRECTORY)
	else()
		set(_CLANG_CL "${SKIA_CLANG_WIN}/bin/clang-cl.exe")
	endif()
	file(TO_CMAKE_PATH "${SKIA_CLANG_WIN}" SKIA_CLANG_WIN)
	message(STATUS "[Skia] Clang-cl : ${_CLANG_CL}")
elseif(_platform STREQUAL "web")
	if(NOT DEFINED SKIA_EMSDK_DIR)
		set(SKIA_EMSDK_DIR "")
	endif()
	emsdk_resolve("${SKIA_EMSDK_DIR}" SKIA_EMSDK_DIR)
	message(STATUS "[Skia] Emsdk    : ${SKIA_EMSDK_DIR}")
else()
	if(NOT DEFINED SKIA_ANDROID_NDK_DIR)
		set(SKIA_ANDROID_NDK_DIR "")
	endif()
	android_resolve_ndk("${SKIA_ANDROID_NDK_DIR}" SKIA_ANDROID_NDK_DIR)
	# git-sync-deps never installs an NDK, unlike the emsdk, so validate before the slow steps rather than after.
	android_require_ndk("${SKIA_ANDROID_NDK_DIR}")
	message(STATUS "[Skia] Ndk      : ${SKIA_ANDROID_NDK_DIR}")
endif()

if(NOT DEFINED SKIA_BUILD_SYNC_DEPS)
	set(SKIA_BUILD_SYNC_DEPS ON)
endif()
if(SKIA_BUILD_SYNC_DEPS)
	message(STATUS "[Skia] Sync deps...")
	execute_process(
		COMMAND "${PYTHON}" "${SKIA_SRC}/tools/git-sync-deps"
		WORKING_DIRECTORY "${SKIA_SRC}"
		RESULT_VARIABLE _sync_rc)
	if(NOT _sync_rc EQUAL 0)
		message(FATAL_ERROR "[Skia] Sync deps failed (rc=${_sync_rc})")
	endif()
else()
	message(STATUS "[Skia] Sync deps skipped (SKIA_BUILD_SYNC_DEPS=OFF)")
endif()

# Each patch covers something gn exposes no argument for: the ICU data profile Skia hardcodes per platform, and a pair of upstream defines whose angle brackets cmd.exe reads as redirection when ninja shells out on Windows.
# Both apply on every host, because both describe the build rather than the machine running it: the freetype one only drops defines that the include order already resolves to the same headers.
file(GLOB _patches "${CMAKE_CURRENT_LIST_DIR}/patch/*.patch")
list(SORT _patches)

if(_patches)
	find_program(GIT NAMES git)
	if(NOT GIT)
		message(FATAL_ERROR "[Skia] Git not found")
	endif()
endif()

foreach(_patch IN LISTS _patches)
	get_filename_component(_patch_name "${_patch}" NAME)

	# A patch that reverses cleanly is already in the tree, so applying it again would fail.
	execute_process(
		COMMAND "${GIT}" apply --check --reverse "${_patch}"
		WORKING_DIRECTORY "${SKIA_SRC}"
		RESULT_VARIABLE _patch_rc
		OUTPUT_QUIET ERROR_QUIET)
	if(_patch_rc EQUAL 0)
		continue()
	endif()

	execute_process(
		COMMAND "${GIT}" apply "${_patch}"
		WORKING_DIRECTORY "${SKIA_SRC}"
		RESULT_VARIABLE _patch_rc)
	if(NOT _patch_rc EQUAL 0)
		message(FATAL_ERROR "[Skia] Patch failed (rc=${_patch_rc}): ${_patch_name}")
	endif()
	message(STATUS "[Skia] Patch applied: ${_patch_name}")
endforeach()

if(NOT EXISTS "${_gn_bundled}")
	message(STATUS "[Skia] Fetch gn...")
	execute_process(
		COMMAND "${PYTHON}" "${SKIA_SRC}/bin/fetch-gn"
		WORKING_DIRECTORY "${SKIA_SRC}"
		RESULT_VARIABLE _fetchgn_rc)
	if(NOT _fetchgn_rc EQUAL 0)
		message(FATAL_ERROR "[Skia] Fetch gn failed (rc=${_fetchgn_rc})")
	endif()
endif()

if(EXISTS "${_gn_bundled}")
	set(GN "${_gn_bundled}")
else()
	find_program(GN NAMES gn)
	if(NOT GN)
		message(FATAL_ERROR "[Skia] Gn not found")
	endif()
endif()

if(_platform STREQUAL "web")
	emsdk_require_compiler("${SKIA_EMSDK_DIR}")
endif()

if(SKIA_BUILD_CLEAN AND EXISTS "${SKIA_OUT}")
	message(STATUS "[Skia] Cleaning: ${SKIA_OUT}")
	file(REMOVE_RECURSE "${SKIA_OUT}")
endif()

file(MAKE_DIRECTORY "${SKIA_OUT}")
file(READ "${_gn_args_platform}" _args_platform_body)
file(READ "${_gn_args_common}" _args_common_body)
file(WRITE "${SKIA_OUT}/args.gn" "${_args_platform_body}\n${_args_common_body}")

if(_cfg STREQUAL "release")
	set(_is_official_build "true")
	set(_is_debug "false")
else()
	set(_is_official_build "false")
	set(_is_debug "true")
endif()

set(_gn_env
	"SKIA_TARGET_CPU=${_gn_cpu}"
	"SKIA_IS_OFFICIAL_BUILD=${_is_official_build}"
	"SKIA_IS_DEBUG=${_is_debug}")
if(_platform STREQUAL "windows")
	if(_cfg STREQUAL "debug")
		set(_win_crt "/MDd")
	else()
		set(_win_crt "/MD")
	endif()
	list(APPEND _gn_env
		"SKIA_CLANG_WIN=${SKIA_CLANG_WIN}"
		"SKIA_WIN_CRT=${_win_crt}")
elseif(_platform STREQUAL "web")
	list(APPEND _gn_env "SKIA_EMSDK_DIR=${SKIA_EMSDK_DIR}")
else()
	list(APPEND _gn_env "SKIA_ANDROID_NDK_DIR=${SKIA_ANDROID_NDK_DIR}")
endif()

message(STATUS "[Skia] Running gn...")
execute_process(
	COMMAND "${CMAKE_COMMAND}" -E env ${_gn_env}
		"${GN}" gen "${SKIA_OUT}" "--script-executable=${PYTHON}"
	WORKING_DIRECTORY "${SKIA_SRC}"
	RESULT_VARIABLE _gen_rc)
if(NOT _gen_rc EQUAL 0)
	message(FATAL_ERROR "[Skia] Gn gen failed (rc=${_gen_rc})")
endif()

if(_platform STREQUAL "windows")
	set(_prefix "")
	set(_suffix ".lib")
else()
	set(_prefix "lib")
	set(_suffix ".a")
endif()
set(_ninja_targets "")
foreach(_mod IN LISTS SKIA_MODULES)
	list(APPEND _ninja_targets "${_prefix}${_mod}${_suffix}")
endforeach()

message(STATUS "[Skia] Running ninja...")
set(_ninja_cmd "${NINJA}" -C "${SKIA_OUT}" ${_ninja_targets})
if(SKIA_BUILD_JOBS)
	list(APPEND _ninja_cmd -j "${SKIA_BUILD_JOBS}")
endif()
execute_process(
	COMMAND ${_ninja_cmd}
	WORKING_DIRECTORY "${SKIA_SRC}"
	RESULT_VARIABLE _build_rc)
if(NOT _build_rc EQUAL 0)
	message(FATAL_ERROR "[Skia] Ninja failed (rc=${_build_rc})")
endif()

foreach(_lib IN LISTS _ninja_targets)
	if(NOT EXISTS "${SKIA_OUT}/${_lib}")
		message(FATAL_ERROR "[Skia] Product missing: ${SKIA_OUT}/${_lib}")
	endif()
endforeach()

message(STATUS "[Skia] Staging products...")
file(MAKE_DIRECTORY "${SKIA_STAGE}")
foreach(_lib IN LISTS _ninja_targets)
	file(COPY "${SKIA_OUT}/${_lib}" DESTINATION "${SKIA_STAGE}")
endforeach()

# Copy every ICU data profile.
set(_icu_externals "${SKIA_SRC}/third_party/externals/icu")
set(_icu_stage "${_repo_root}/out/third-party/skia/icu")
file(GLOB _icu_profile_dats "${_icu_externals}/*/icudtl.dat")
if(_icu_profile_dats)
	foreach(_dat IN LISTS _icu_profile_dats)
		get_filename_component(_profile_dir "${_dat}" DIRECTORY)
		get_filename_component(_profile "${_profile_dir}" NAME)
		file(COPY "${_dat}" DESTINATION "${_icu_stage}/${_profile}")
		message(STATUS "[Skia] ICU profile staged: ${_profile}")
	endforeach()
else()
	message(FATAL_ERROR "[Skia] No ICU profile data found under ${_icu_externals}")
endif()

message(STATUS "[Skia] Done: ${SKIA_STAGE}")
