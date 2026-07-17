include_guard(GLOBAL)

set(GLAZE_SRC "${CMAKE_SOURCE_DIR}/third-party/glaze" CACHE PATH "Glaze submodule source dir")

if(NOT EXISTS "${GLAZE_SRC}/include/glaze/glaze.hpp")
	message(FATAL_ERROR "[Glaze] Submodule not found: ${GLAZE_SRC}")
endif()

message(STATUS "[Glaze] ${GLAZE_SRC}/include")

# Glaze uses C++23 compile-time reflection, so consumers of this target compile as C++23.
add_library(glaze_glaze INTERFACE)
add_library(glaze::glaze ALIAS glaze_glaze)

target_include_directories(glaze_glaze SYSTEM INTERFACE "${GLAZE_SRC}/include")
target_compile_features(glaze_glaze INTERFACE cxx_std_23)
