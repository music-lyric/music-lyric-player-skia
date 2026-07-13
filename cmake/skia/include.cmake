# Import prebuilt Skia static libraries (built by cmake/skia/build.cmake).
# Exports target `skia` (alias `skia::skia`).
# Must be included before any target is defined (sets CRT globally on MSVC).

include_guard(GLOBAL)

# Modules
set(SKIA_MODULES skia skshaper skunicode_core skunicode_icu)

# Skia root
set(SKIA_SRC "${CMAKE_SOURCE_DIR}/third-party/skia" CACHE PATH "Skia submodule source dir")

if(NOT EXISTS "${SKIA_SRC}/BUILD.gn")
	message(FATAL_ERROR "Skia not found at: ${SKIA_SRC}")
endif()

# Locate prebuilt products
if(NOT DEFINED SKIA_BUILD_CONFIG)
	set(SKIA_BUILD_CONFIG "Release")
endif()

if(DEFINED SKIA_OUT_DIR)
	set(SKIA_OUT "${SKIA_OUT_DIR}")
else()
	string(TOLOWER "${SKIA_BUILD_CONFIG}" _cfg)
	set(SKIA_OUT "${SKIA_SRC}/out/${_cfg}")
endif()

# Resolve module libs under SKIA_OUT into SKIA_LIBRARIES; leftover names go to _missing.
set(SKIA_LIBRARIES "")
set(_missing "")
foreach(_mod IN LISTS SKIA_MODULES)
	set(_file "${SKIA_OUT}/${CMAKE_STATIC_LIBRARY_PREFIX}${_mod}${CMAKE_STATIC_LIBRARY_SUFFIX}")
	if(EXISTS "${_file}")
		list(APPEND SKIA_LIBRARIES "${_file}")
	else()
		list(APPEND _missing "${_mod}")
	endif()
endforeach()

if(_missing)
	string(REPLACE ";" "\n    " _missing_text "${_missing}")
	message(FATAL_ERROR "[Skia] Libraries not found under: ${SKIA_OUT}")
endif()

message(STATUS "[Skia] ${SKIA_OUT}")

# System libraries
if(WIN32)
	set(SKIA_SYSTEM_LIBRARIES Ole32 OleAut32 User32 Gdi32 Usp10 FontSub Advapi32)
elseif(APPLE)
	set(SKIA_SYSTEM_LIBRARIES "-framework CoreFoundation" "-framework CoreGraphics" "-framework CoreText")
else()
	set(SKIA_SYSTEM_LIBRARIES fontconfig freetype dl pthread)
endif()

# Export target
add_library(skia INTERFACE)
add_library(skia::skia ALIAS skia)

set(_skia_incs "${SKIA_SRC}")
if(EXISTS "${SKIA_OUT}/gen")
	list(APPEND _skia_incs "${SKIA_OUT}/gen")
endif()

target_include_directories(skia INTERFACE ${_skia_incs})

target_link_libraries(skia INTERFACE ${SKIA_LIBRARIES} ${SKIA_SYSTEM_LIBRARIES})

target_compile_definitions(skia INTERFACE SK_GANESH SK_VULKAN)
target_compile_features(skia INTERFACE cxx_std_20)
