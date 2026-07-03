include(FetchContent)

# GLFW (window + Vulkan surface + input). Fetched, not vendored.
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

# Vulkan headers + loader. The user installs the SDK; VULKAN_SDK drives discovery.
find_package(Vulkan REQUIRED)

add_executable(music_lyric_player_example_windows
	main.cpp
	sample_lyric.cpp
	vulkan_window.cpp
)

target_compile_features(music_lyric_player_example_windows PRIVATE cxx_std_20)

target_include_directories(music_lyric_player_example_windows PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

# music_lyric_player::render transitively brings playback, the lyric model (with its /utf-8 flag) and skia.
target_link_libraries(music_lyric_player_example_windows PRIVATE
	music_lyric_player::render
	glfw
	Vulkan::Vulkan)

# SkParagraph's ICU backend memory-maps icudtl.dat from the executable's directory at runtime, so
# copy Skia's data file next to the binary after each build.
add_custom_command(TARGET music_lyric_player_example_windows POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${SKIA_OUT}/icudtl.dat"
		"$<TARGET_FILE_DIR:music_lyric_player_example_windows>/icudtl.dat"
	COMMENT "Copying icudtl.dat next to the example executable")
