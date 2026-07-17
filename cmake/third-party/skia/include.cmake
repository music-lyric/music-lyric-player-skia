include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/../../common/platform.cmake")

set(SKIA_MODULES skia skshaper skunicode_core skunicode_icu)

set(SKIA_SRC "${CMAKE_SOURCE_DIR}/third-party/skia" CACHE PATH "Skia submodule source dir")

if(NOT EXISTS "${SKIA_SRC}/BUILD.gn")
	message(FATAL_ERROR "[Skia] Submodule not found: ${SKIA_SRC}")
endif()

if(NOT DEFINED SKIA_BUILD_CONFIG)
	set(SKIA_BUILD_CONFIG "Release")
endif()

if(NOT DEFINED SKIA_BUILD_PLATFORM)
	if(ANDROID)
		set(SKIA_BUILD_PLATFORM "android")
	elseif(EMSCRIPTEN OR CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
		set(SKIA_BUILD_PLATFORM "web")
	elseif(WIN32)
		set(SKIA_BUILD_PLATFORM "windows")
	else()
		message(FATAL_ERROR "[Skia] Unsupported target platform: ${CMAKE_SYSTEM_NAME}")
	endif()
endif()
string(TOLOWER "${SKIA_BUILD_PLATFORM}" _skia_platform_input)

if(NOT DEFINED SKIA_BUILD_ARCH)
	if(_skia_platform_input STREQUAL "android")
		if(DEFINED ANDROID_ABI AND NOT ANDROID_ABI STREQUAL "")
			set(SKIA_BUILD_ARCH "${ANDROID_ABI}")
		else()
			set(SKIA_BUILD_ARCH "arm64-v8a")
		endif()
	elseif(_skia_platform_input STREQUAL "web")
		set(SKIA_BUILD_ARCH "wasm32")
	else()
		set(SKIA_BUILD_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
		if(SKIA_BUILD_ARCH STREQUAL "")
			set(SKIA_BUILD_ARCH "x64")
		endif()
	endif()
endif()

platform_resolve_target(
	"${SKIA_BUILD_PLATFORM}" "${SKIA_BUILD_ARCH}" "${SKIA_BUILD_CONFIG}"
	_skia_platform _skia_arch _skia_cfg)

set(SKIA_PLATFORM "${_skia_platform}")
set(SKIA_ARCH "${_skia_arch}")

set(_skia_out_name "${_skia_platform}-${_skia_arch}-${_skia_cfg}")
if(DEFINED SKIA_OUT_DIR)
	set(SKIA_OUT "${SKIA_OUT_DIR}")
else()
	set(SKIA_OUT "${CMAKE_SOURCE_DIR}/out/third-party/skia/${_skia_out_name}")
endif()

set(SKIA_LIBRARIES "")
foreach(_mod IN LISTS SKIA_MODULES)
	set(_file "${SKIA_OUT}/${CMAKE_STATIC_LIBRARY_PREFIX}${_mod}${CMAKE_STATIC_LIBRARY_SUFFIX}")
	if(NOT EXISTS "${_file}")
		message(FATAL_ERROR "[Skia] Prebuilt library not found: ${_file}")
	endif()
	list(APPEND SKIA_LIBRARIES "${_file}")
endforeach()

if(_skia_platform STREQUAL "windows")
	set(SKIA_SYSTEM_LIBRARIES Ole32 OleAut32 User32 Gdi32 Usp10 FontSub Advapi32)
	set(SKIA_COMPILE_DEFINITIONS SK_GANESH SK_VULKAN)
elseif(_skia_platform STREQUAL "android")
	set(SKIA_SYSTEM_LIBRARIES EGL GLESv2 log android dl)
	set(SKIA_COMPILE_DEFINITIONS SK_GANESH SK_GL SK_ASSUME_GL_ES)
else()
	set(SKIA_SYSTEM_LIBRARIES "")
	set(SKIA_COMPILE_DEFINITIONS
		SK_GANESH
		SK_GL
		SK_ASSUME_WEBGL
		SKVX_DISABLE_SIMD
		SK_FORCE_8_BYTE_ALIGNMENT)
endif()

message(STATUS "[Skia] ${SKIA_OUT}")

add_library(skia INTERFACE)
add_library(skia::skia ALIAS skia)

set(_skia_incs "${SKIA_SRC}")
if(EXISTS "${SKIA_OUT}/gen")
	list(APPEND _skia_incs "${SKIA_OUT}/gen")
endif()

target_include_directories(skia INTERFACE ${_skia_incs})
target_link_libraries(skia INTERFACE ${SKIA_LIBRARIES} ${SKIA_SYSTEM_LIBRARIES})
target_compile_definitions(skia INTERFACE ${SKIA_COMPILE_DEFINITIONS})
target_compile_features(skia INTERFACE cxx_std_20)
