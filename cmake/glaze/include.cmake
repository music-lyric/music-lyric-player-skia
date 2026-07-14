# Import the header-only Glaze JSON library as target glaze::glaze.
# Header-only, so there is nothing to prebuild (cmake/glaze/build.cmake is a parity marker only).

include_guard(GLOBAL)

set(GLAZE_SRC "${CMAKE_SOURCE_DIR}/third-party/glaze" CACHE PATH "Glaze submodule source dir")

if(NOT EXISTS "${GLAZE_SRC}/include/glaze/glaze.hpp")
	message(FATAL_ERROR
		"[Glaze] submodule not found at: ${GLAZE_SRC}\n"
		"    Init it: git submodule update --init third-party/glaze")
endif()

message(STATUS "[Glaze] ${GLAZE_SRC}/include")

# Glaze uses C++23 compile-time reflection, so consumers of this target compile as C++23.
add_library(glaze_glaze INTERFACE)
add_library(glaze::glaze ALIAS glaze_glaze)

target_include_directories(glaze_glaze SYSTEM INTERFACE "${GLAZE_SRC}/include")
target_compile_features(glaze_glaze INTERFACE cxx_std_23)
