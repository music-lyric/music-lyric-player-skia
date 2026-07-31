cmake_minimum_required(VERSION 3.21)

if(NOT CMAKE_SCRIPT_MODE_FILE)
	message(FATAL_ERROR "[Build] Must be run in script mode")
endif()

get_filename_component(_repo_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

if(NOT DEFINED BUILD_CONFIG OR BUILD_CONFIG STREQUAL "")
	set(BUILD_CONFIG "Release")
endif()

set(_known_abis "arm64-v8a" "x86_64" "armeabi-v7a" "x86")

if(NOT DEFINED ANDROID_ABI OR ANDROID_ABI STREQUAL "")
	set(ANDROID_ABI "arm64-v8a")
endif()
if(NOT ANDROID_ABI IN_LIST _known_abis)
	list(JOIN _known_abis ", " _known_abis_text)
	message(FATAL_ERROR "[Build] Unsupported Android ABI: ${ANDROID_ABI} (expected one of ${_known_abis_text})")
endif()

# Every ABI gets its own build tree, since one tree cannot hold two sets of cross-compiled objects.
set(_cache_dir "${_repo_root}/out/app/cache/android/${ANDROID_ABI}")

# Routing the toolchain through cmake/tools/android.cmake is what lets the NDK be resolved in CMake code rather than demanded as an environment variable.
set(_toolchain "${_repo_root}/cmake/tools/android.cmake")

message(STATUS "[Build] Target: android ${ANDROID_ABI} (${BUILD_CONFIG})")
message(STATUS "[Build] Cache : ${_cache_dir}")

# ANDROID_PLATFORM and ANDROID_STL are pinned here rather than exposed as arguments.
# The STL has to match what Skia and the lyric model were prebuilt with, since a mismatch links two copies of libc++ into one library.
# The platform is android-24 because that is where Vulkan reached the platform: linking libvulkan below that leaves a library the loader refuses outright.
# ANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES is what supplies the 16 KB page alignment, which the NDK toolchain does not turn on by itself.
message(STATUS "[Build] Configuring...")
execute_process(
	COMMAND "${CMAKE_COMMAND}" -S "${_repo_root}" -B "${_cache_dir}" -G "Ninja"
		"-DCMAKE_TOOLCHAIN_FILE=${_toolchain}" "-DCMAKE_BUILD_TYPE=${BUILD_CONFIG}"
		"-DANDROID_ABI=${ANDROID_ABI}"
		"-DANDROID_PLATFORM=android-24"
		"-DANDROID_STL=c++_static"
		"-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON"
	WORKING_DIRECTORY "${_repo_root}"
	RESULT_VARIABLE _configure_rc)
if(NOT _configure_rc EQUAL 0)
	message(FATAL_ERROR "[Build] Configure failed (rc=${_configure_rc})")
endif()

# The NDK drives a single-config generator, so the build type comes from the cache rather than --config.
message(STATUS "[Build] Building music_lyric_player_android...")
execute_process(
	COMMAND "${CMAKE_COMMAND}" --build "${_cache_dir}" --target music_lyric_player_android
	WORKING_DIRECTORY "${_repo_root}"
	RESULT_VARIABLE _build_rc)
if(NOT _build_rc EQUAL 0)
	message(FATAL_ERROR "[Build] Build failed (rc=${_build_rc})")
endif()

message(STATUS "[Build] Done")
