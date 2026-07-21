include(FetchContent)

# GLFW (window + input). Fetched, not vendored. The backend owns the Vulkan surface, not GLFW.
FetchContent_Declare(
	glfw
	GIT_REPOSITORY https://github.com/glfw/glfw.git
	GIT_TAG 3.4
	GIT_SHALLOW TRUE
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

add_executable(music_lyric_player_example_windows
	lyric_input.cpp
	main.cpp
	window.cpp
)

target_compile_features(music_lyric_player_example_windows PRIVATE cxx_std_20)

# The demo may carry UTF-8 string literals, so the source must be read and encoded as UTF-8.
if(MSVC)
	target_compile_options(music_lyric_player_example_windows PRIVATE /utf-8)
endif()

target_include_directories(music_lyric_player_example_windows PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

set_target_properties(music_lyric_player_example_windows PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out/example/build/${SKIA_PLATFORM}-${SKIA_ARCH}-$<LOWER_CASE:$<CONFIG>>")

# music_lyric_player::rendering transitively brings playback, the lyric model (with its /utf-8 flag) and skia.
# music_lyric_player::backend brings the GPU surface and font manager, and transitively Vulkan.
# music_lyric_player::icu embeds the ICU data and SkLoadICU override so no external icudtl.dat is needed.
target_link_libraries(music_lyric_player_example_windows PRIVATE
	music_lyric_player::backend
	music_lyric_player::rendering
	music_lyric_player::icu
	glfw)
