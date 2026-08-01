cmake_minimum_required(VERSION 3.21)

if(NOT CMAKE_SCRIPT_MODE_FILE)
	message(FATAL_ERROR "[Package] Must be run in script mode")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../common/android.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../common/platform.cmake")

get_filename_component(_repo_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

# The distribution ships Release only, which is the sole configuration the Android module is built in.
if(NOT DEFINED PACKAGE_CONFIG OR PACKAGE_CONFIG STREQUAL "")
	set(PACKAGE_CONFIG "Release")
endif()

# One aar carries every ABI, so the list is what the package is built from rather than something the caller picks one of.
if(NOT DEFINED PACKAGE_ABIS OR PACKAGE_ABIS STREQUAL "")
	set(PACKAGE_ABIS "arm64-v8a;x86_64")
endif()

if(NOT PACKAGE_CONFIG STREQUAL "Release")
	message(FATAL_ERROR "[Package] Only Release is packaged, got: ${PACKAGE_CONFIG}")
endif()

set(_module_dir "${_repo_root}/platform/android")
set(_jni_libs "${_module_dir}/src/main/jniLibs")
set(_dist_dir "${_repo_root}/out/app/dist")

# VERSION.txt is the one source of the release, and script/release.py rewrites the Gradle property from it in the same commit.
# A split between the two would ship an aar whose file name and POM disagree, so it fails here rather than at publish time.
file(STRINGS "${_repo_root}/VERSION.txt" _version LIMIT_COUNT 1)
string(STRIP "${_version}" _version)

file(STRINGS "${_module_dir}/gradle.properties" _version_lines REGEX "^VERSION_NAME[ \t]*=")
if(NOT _version_lines)
	message(FATAL_ERROR "[Package] gradle.properties declares no VERSION_NAME")
endif()
list(GET _version_lines 0 _version_line)
string(REGEX REPLACE "^VERSION_NAME[ \t]*=[ \t]*" "" _module_version "${_version_line}")
string(STRIP "${_module_version}" _module_version)
if(NOT _module_version STREQUAL "${_version}")
	message(FATAL_ERROR "[Package] VERSION.txt is ${_version} but the module declares ${_module_version}")
endif()

android_resolve_ndk("" _ndk_dir)
android_require_ndk("${_ndk_dir}")
android_resolve_llvm_tool("${_ndk_dir}" "llvm-readelf" _readelf)

message(STATUS "[Package] Target : android ${PACKAGE_ABIS} (${PACKAGE_CONFIG})")
message(STATUS "[Package] Module : ${_module_dir}")
message(STATUS "[Package] Version: ${_version}")

# Resolve the whole list before anything is touched, so a typo in it cannot leave the module staged half way.
# The config resolves to the same token every time, since only Release gets this far.
set(_arches "")
foreach(_abi IN LISTS PACKAGE_ABIS)
	platform_resolve_target("android" "${_abi}" "${PACKAGE_CONFIG}" _platform _arch _cfg)
	list(APPEND _arches "${_arch}")
endforeach()
list(REMOVE_DUPLICATES _arches)

# Replace jniLibs wholesale so an ABI dropped from the list cannot survive as a stale library inside the aar.
message(STATUS "[Package] Staging...")
file(REMOVE_RECURSE "${_jni_libs}")

foreach(_arch IN LISTS _arches)
	set(_triple "${_platform}-${_arch}-${_cfg}")
	set(_library "${_repo_root}/out/app/build/${_triple}/libmusic_lyric_player.so")

	# Build the library the same way the project does, unless it is already built.
	# The build script owns how the module is configured, so packaging and `make android-build` cannot drift apart.
	if(NOT PACKAGE_SKIP_BUILD)
		set(BUILD_CONFIG "${PACKAGE_CONFIG}")
		set(ANDROID_ABI "${_arch}")
		include("${CMAKE_CURRENT_LIST_DIR}/../build/android.cmake")
	endif()

	if(NOT EXISTS "${_library}")
		message(FATAL_ERROR "[Package] Missing build artifact: ${_library}")
	endif()

	# The 16 KB page size is what recent devices load with, and a library aligned to the old 4 KB simply refuses to install there.
	# The NDK turns it on by itself from r28, so this guards against a silent regression that only a 16 KB device would ever show.
	execute_process(
		COMMAND "${_readelf}" -l "${_library}"
		OUTPUT_VARIABLE _headers
		RESULT_VARIABLE _readelf_rc)
	if(NOT _readelf_rc EQUAL 0)
		message(FATAL_ERROR "[Package] Reading the program headers failed (rc=${_readelf_rc}): ${_library}")
	endif()

	string(REPLACE "\r" "" _headers "${_headers}")
	string(REGEX MATCHALL "LOAD[^\n]*" _segments "${_headers}")
	if(NOT _segments)
		message(FATAL_ERROR "[Package] No LOAD segment found in ${_library}")
	endif()

	foreach(_segment IN LISTS _segments)
		# The alignment is the last column of the segment line, and reading it as a number covers both the hex and the decimal spelling.
		string(REGEX MATCH "[0-9a-fA-Fx]+$" _align "${_segment}")
		math(EXPR _align_value "${_align}" OUTPUT_FORMAT DECIMAL)
		if(NOT _align_value EQUAL 16384)
			message(FATAL_ERROR "[Package] LOAD segment of ${_arch} is aligned to ${_align}, not 16 KB")
		endif()
	endforeach()

	message(STATUS "[Package] ${_arch}: 16 KB aligned")
	file(COPY "${_library}" DESTINATION "${_jni_libs}/${_arch}")
endforeach()

# Gradle only packages what CMake staged, so it runs once after every ABI is in place.
if(CMAKE_HOST_WIN32)
	set(_gradlew "${_module_dir}/gradlew.bat")
else()
	set(_gradlew "${_module_dir}/gradlew")
endif()

message(STATUS "[Package] Assembling the aar...")
execute_process(
	COMMAND "${_gradlew}" ":assembleRelease" "--console=plain"
	WORKING_DIRECTORY "${_module_dir}"
	RESULT_VARIABLE _gradle_rc)
if(NOT _gradle_rc EQUAL 0)
	message(FATAL_ERROR "[Package] Assembling the aar failed (rc=${_gradle_rc})")
endif()

# The aar is named after the Gradle root project, so it is matched rather than spelled out and a rename stays a one-line change over there.
file(GLOB _aars "${_module_dir}/build/outputs/aar/*-release.aar")
list(LENGTH _aars _aar_count)
if(NOT _aar_count EQUAL 1)
	message(FATAL_ERROR "[Package] Expected one release aar, found ${_aar_count} in ${_module_dir}/build/outputs/aar")
endif()
list(GET _aars 0 _aar)

get_filename_component(_aar_name "${_aar}" NAME)
string(REGEX REPLACE "-release\\.aar$" "" _artifact_name "${_aar_name}")

# The aar already holds every ABI, so it ships as one versioned file rather than under the {platform}-{arch}-{config} triple the zips use.
set(_output "${_dist_dir}/${_artifact_name}-${_version}.aar")
file(MAKE_DIRECTORY "${_dist_dir}")
file(REMOVE "${_output}")
file(COPY_FILE "${_aar}" "${_output}")

message(STATUS "[Package] Done: ${_output}")
