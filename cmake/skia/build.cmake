# Standalone Skia build script. Run via: cmake -P cmake/skia/build.cmake
# Optional args (before -P):
#   -D SKIA_BUILD_CONFIG=Debug|Release
#   -D SKIA_BUILD_CLEAN=ON
#   -D SKIA_BUILD_JOBS=8

cmake_minimum_required(VERSION 3.21)

if(NOT CMAKE_SCRIPT_MODE_FILE)
	message(FATAL_ERROR
		"This is a standalone script. Run it with:\n"
		"    cmake -P cmake/skia/build.cmake")
endif()

# Modules to build
set(SKIA_MODULES skia skparagraph skshaper skunicode_core skunicode_icu)

# Parameters
if(NOT DEFINED SKIA_BUILD_CONFIG)
	set(SKIA_BUILD_CONFIG "Release")
endif()

get_filename_component(_repo_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(SKIA_SRC "${_repo_root}/third-party/skia")
string(TOLOWER "${SKIA_BUILD_CONFIG}" _cfg)
set(SKIA_OUT "${SKIA_SRC}/out/skia-${_cfg}")

message(STATUS "[Skia] Start Skia Build...")
message(STATUS "[Skia] Source : ${SKIA_SRC}")
message(STATUS "[Skia] Output : ${SKIA_OUT}")
message(STATUS "[Skia] Config : ${SKIA_BUILD_CONFIG}")

# Submodule check
if(NOT EXISTS "${SKIA_SRC}/BUILD.gn")
	message(FATAL_ERROR
		"Skia submodule not found at: ${SKIA_SRC}\n"
		"    git submodule update --init third-party/skia")
endif()

# Toolchain detection
find_program(PYTHON NAMES python python3)
if(NOT PYTHON)
	message(FATAL_ERROR "python not found on PATH.")
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
	message(FATAL_ERROR "ninja not found on PATH.")
endif()

if(CMAKE_HOST_WIN32)
	set(_gn_bundled "${SKIA_SRC}/bin/gn.exe")
else()
	set(_gn_bundled "${SKIA_SRC}/bin/gn")
endif()

# Windows: Skia builds with clang-cl. Require it on PATH.
if(CMAKE_HOST_WIN32)
	find_program(_CLANG_CL NAMES clang-cl)
	if(NOT _CLANG_CL)
		message(FATAL_ERROR
			"Skia needs clang-cl on Windows but it was not found on PATH.\n"
			"Install LLVM and add its bin/ to PATH.")
	endif()
	# LLVM root is the parent of bin/ (…/LLVM/bin/clang-cl.exe -> …/LLVM)
	get_filename_component(_bin_dir "${_CLANG_CL}" DIRECTORY)
	get_filename_component(SKIA_CLANG_WIN "${_bin_dir}" DIRECTORY)
	message(STATUS "[Skia] clang-cl : ${_CLANG_CL}")
endif()

# git-sync-deps
message(STATUS "[Skia] Sync deps...")
execute_process(
	COMMAND "${PYTHON}" "${SKIA_SRC}/tools/git-sync-deps"
	WORKING_DIRECTORY "${SKIA_SRC}"
	RESULT_VARIABLE _sync_rc)
if(NOT _sync_rc EQUAL 0)
	message(FATAL_ERROR "git-sync-deps failed (rc=${_sync_rc})")
endif()

# Ensure gn
if(NOT EXISTS "${_gn_bundled}")
	message(STATUS "[Skia] Fetch gn...")
	execute_process(
		COMMAND "${PYTHON}" "${SKIA_SRC}/bin/fetch-gn"
		WORKING_DIRECTORY "${SKIA_SRC}"
		RESULT_VARIABLE _fetchgn_rc)
	if(NOT _fetchgn_rc EQUAL 0)
		message(FATAL_ERROR "fetch-gn failed (rc=${_fetchgn_rc})")
	endif()
endif()

if(EXISTS "${_gn_bundled}")
	set(GN "${_gn_bundled}")
else()
	find_program(GN NAMES gn)
	if(NOT GN)
		message(FATAL_ERROR "gn not found.")
	endif()
endif()

# Cleaning
if(SKIA_BUILD_CLEAN AND EXISTS "${SKIA_OUT}")
	message(STATUS "[Skia] Cleaning: ${SKIA_OUT}")
	file(REMOVE_RECURSE "${SKIA_OUT}")
endif()

# GN args
set(_gn_args
	"is_official_build = true"
	"skia_enable_ganesh = true"
	"skia_use_gl = true"
	"skia_use_vulkan = false"
	"skia_use_direct3d = false"
	"skia_use_metal = false"
	"skia_use_harfbuzz = true"
	"skia_use_icu = true"
	"skia_use_freetype = true"
	"skia_use_system_harfbuzz = false"
	"skia_use_system_icu = false"
	"skia_use_system_freetype2 = false"
	"skia_use_system_zlib = false"
	"skia_use_system_libpng = false"
	"skia_use_system_libjpeg_turbo = false"
	"skia_use_system_libwebp = false"
	"skia_use_system_expat = false"
	"skia_enable_skparagraph = true"
	"skia_enable_svg = false"
	"skia_enable_tools = false"
)

if(CMAKE_HOST_WIN32)
	list(APPEND _gn_args "clang_win = \"${SKIA_CLANG_WIN}\"")
else()
	list(APPEND _gn_args "cc = \"clang\"" "cxx = \"clang++\"")
endif()

# gn gen
file(MAKE_DIRECTORY "${SKIA_OUT}")
list(JOIN _gn_args "\n" _gn_args_text)
file(WRITE "${SKIA_OUT}/args.gn" "${_gn_args_text}\n")

message(STATUS "[Skia] Running gn...")
execute_process(
	COMMAND "${GN}" gen "${SKIA_OUT}" "--script-executable=${PYTHON}"
	WORKING_DIRECTORY "${SKIA_SRC}"
	RESULT_VARIABLE _gen_rc)
if(NOT _gen_rc EQUAL 0)
	message(FATAL_ERROR "gn gen failed (rc=${_gen_rc})")
endif()

# ninja
if(CMAKE_HOST_WIN32)
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

message(STATUS "[Skia] Running ninja")
set(_ninja_cmd "${NINJA}" -C "${SKIA_OUT}" ${_ninja_targets})
if(SKIA_BUILD_JOBS)
	list(APPEND _ninja_cmd -j "${SKIA_BUILD_JOBS}")
endif()
execute_process(
	COMMAND ${_ninja_cmd}
	WORKING_DIRECTORY "${SKIA_SRC}"
	RESULT_VARIABLE _build_rc)
if(NOT _build_rc EQUAL 0)
	message(FATAL_ERROR "ninja failed (rc=${_build_rc})")
endif()

# Verify
foreach(_lib IN LISTS _ninja_targets)
	if(NOT EXISTS "${SKIA_OUT}/${_lib}")
		message(FATAL_ERROR "Product missing: ${SKIA_OUT}/${_lib}")
	endif()
endforeach()

message(STATUS "[Skia] Skia build complete.")
