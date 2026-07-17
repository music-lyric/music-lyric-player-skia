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
	main.cpp
	sample_lyric.cpp
	window.cpp
)

target_compile_features(music_lyric_player_example_windows PRIVATE cxx_std_20)

target_include_directories(music_lyric_player_example_windows PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

set_target_properties(music_lyric_player_example_windows PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out/example/${SKIA_PLATFORM}-${SKIA_ARCH}-$<LOWER_CASE:$<CONFIG>>")

# music_lyric_player::rendering transitively brings playback, the lyric model (with its /utf-8 flag) and skia.
# music_lyric_player::backend brings the GPU surface and font manager, and transitively Vulkan.
target_link_libraries(music_lyric_player_example_windows PRIVATE
	music_lyric_player::backend
	music_lyric_player::rendering
	glfw)

# Skia's ICU backend (SkUnicode) memory-maps icudtl.dat from the executable's directory at runtime.
# Copy Skia's data file next to the binary after each build.
add_custom_command(TARGET music_lyric_player_example_windows POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${SKIA_OUT}/icudtl.dat"
		"$<TARGET_FILE_DIR:music_lyric_player_example_windows>/icudtl.dat"
	COMMENT "Copying icudtl.dat next to the example executable")
