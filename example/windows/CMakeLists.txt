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

# miniaudio (audio decode + playback) is a single header, pinned as a submodule next to the example sources.
set(MINIAUDIO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third-party/miniaudio")
if(NOT EXISTS "${MINIAUDIO_DIR}/miniaudio.h")
	message(FATAL_ERROR "miniaudio is missing. Run: git submodule update --init example/third-party/miniaudio")
endif()

# Dear ImGui drives the control panel; only its core plus the GLFW input backend are compiled, the renderer is ours.
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third-party/imgui")
if(NOT EXISTS "${IMGUI_DIR}/imgui.h")
	message(FATAL_ERROR "Dear ImGui is missing. Run: git submodule update --init example/third-party/imgui")
endif()

add_executable(music_lyric_player_example_windows
	audio.cpp
	imgui_skia.cpp
	lyric_input.cpp
	main.cpp
	miniaudio.cpp
	panel.cpp
	settings.cpp
	state.cpp
	text.cpp
	theme.cpp
	widgets.cpp
	window.cpp
	"${IMGUI_DIR}/imgui.cpp"
	"${IMGUI_DIR}/imgui_draw.cpp"
	"${IMGUI_DIR}/imgui_tables.cpp"
	"${IMGUI_DIR}/imgui_widgets.cpp"
	"${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
)

# Glaze reflects the state struct into json and requires C++23.
target_compile_features(music_lyric_player_example_windows PRIVATE cxx_std_23)

# The demo only plays local files, so leave out miniaudio's encoding and waveform-synthesis halves.
# NOMINMAX stops windows.h from defining min/max as macros, which would otherwise break std::min / std::max and every heavily templated header such as Glaze or Skia that a translation unit pulls in after it.
# The sidebar prints the release the demo was built from, which VERSION.txt is the only source of.
target_compile_definitions(music_lyric_player_example_windows PRIVATE
	MA_NO_ENCODING
	MA_NO_GENERATION
	NOMINMAX
	MUSIC_LYRIC_PLAYER_VERSION="${MUSIC_LYRIC_PLAYER_VERSION}")

# The demo may carry UTF-8 string literals, so the source must be read and encoded as UTF-8.
if(MSVC)
	target_compile_options(music_lyric_player_example_windows PRIVATE /utf-8)
	# miniaudio is third-party C compiled as one large translation unit; its warnings are not actionable here.
	set_source_files_properties(miniaudio.cpp PROPERTIES COMPILE_OPTIONS "/W0")
endif()

target_include_directories(music_lyric_player_example_windows PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}"
	"${MINIAUDIO_DIR}"
	"${IMGUI_DIR}")

set_target_properties(music_lyric_player_example_windows PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out/example/build/${SKIA_PLATFORM}-${SKIA_ARCH}-$<LOWER_CASE:$<CONFIG>>")

# music_lyric_player::rendering transitively brings playback, the lyric model (with its /utf-8 flag) and skia.
# music_lyric_player::backend brings the GPU surface and font manager, and transitively Vulkan.
# music_lyric_player::icu embeds the ICU data and SkLoadICU override so no external icudtl.dat is needed.
# comdlg32 provides the file picker used to open an audio track.
target_link_libraries(music_lyric_player_example_windows PRIVATE
	music_lyric_player::backend
	music_lyric_player::rendering
	music_lyric_player::icu
	glaze::glaze
	comdlg32
	glfw)
