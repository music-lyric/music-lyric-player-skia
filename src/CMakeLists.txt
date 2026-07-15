find_package(Vulkan REQUIRED)

add_library(music_lyric_player_playback
	utils/clock/steady.cpp
	playback/merger.cpp
	playback/offset.cpp
	playback/player.cpp
)
add_library(music_lyric_player::playback ALIAS music_lyric_player_playback)

target_compile_features(music_lyric_player_playback PUBLIC cxx_std_20)

target_include_directories(music_lyric_player_playback PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(music_lyric_player_playback PUBLIC
	music_lyric::model)

add_library(music_lyric_player_rendering
	rendering/components/line/normal/index.cpp
	rendering/components/line/normal/main/plain/index.cpp
	rendering/components/line/normal/main/syllable/index.cpp
	rendering/components/line/normal/main/syllable/word.cpp
	rendering/components/line/normal/main/syllable/animation/float.cpp
	rendering/components/line/normal/main/syllable/animation/mask.cpp
	rendering/components/line/interlude/index.cpp
	rendering/core/effect.cpp
	rendering/core/layout.cpp
	rendering/core/line.cpp
	rendering/core/scroll.cpp
	rendering/core/transition.cpp
	rendering/renderer.cpp
)
add_library(music_lyric_player::rendering ALIAS music_lyric_player_rendering)

target_compile_features(music_lyric_player_rendering PUBLIC cxx_std_20)

target_include_directories(music_lyric_player_rendering PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(music_lyric_player_rendering PUBLIC
	music_lyric_player::playback
	music_lyric::model
	skia::skia)

add_library(music_lyric_player_backend
	backend/gpu/vulkan.cpp
	backend/font/font_win.cpp
)
add_library(music_lyric_player::backend ALIAS music_lyric_player_backend)

target_compile_features(music_lyric_player_backend PUBLIC cxx_std_20)

target_include_directories(music_lyric_player_backend PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(music_lyric_player_backend PUBLIC
	skia::skia
	Vulkan::Vulkan)

add_library(music_lyric_player_native SHARED
	platform/native/abi.cpp
)
add_library(music_lyric_player::native ALIAS music_lyric_player_native)

target_compile_features(music_lyric_player_native PUBLIC cxx_std_20)

# Building the shared library exports the public C ABI as dllexport through MUSIC_LYRIC_PLAYER_API.
# Every other symbol, from Skia and ICU to protobuf and the C++ runtime, stays hidden inside the DLL.
target_compile_definitions(music_lyric_player_native PRIVATE MUSIC_LYRIC_PLAYER_BUILDING)

# Non-MSVC toolchains need these presets to hide unmarked symbols, which MSVC already does by default.
# They pair with the visibility("default") branch in export.h so the narrow ABI holds on every platform.
set_target_properties(music_lyric_player_native PROPERTIES
	C_VISIBILITY_PRESET hidden
	CXX_VISIBILITY_PRESET hidden
	VISIBILITY_INLINES_HIDDEN ON)

target_include_directories(music_lyric_player_native PUBLIC "${CMAKE_SOURCE_DIR}/include")

# The engine and Glaze are absorbed statically into the DLL and never surface in the C ABI headers.
# Every dependency stays PRIVATE, so consumers link the DLL alone and see only the include/ headers.
# Glaze additionally raises this target to its required C++23 without imposing that on consumers.
target_link_libraries(music_lyric_player_native PRIVATE
	music_lyric_player::backend
	music_lyric_player::rendering
	music_lyric_player::playback
	music_lyric::model
	glaze::glaze)
