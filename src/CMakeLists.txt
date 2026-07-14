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

add_library(music_lyric_player_render
	render/components/line/normal/index.cpp
	render/components/line/normal/main/plain/index.cpp
	render/components/line/normal/main/syllable/index.cpp
	render/components/line/normal/main/syllable/word.cpp
	render/components/line/normal/main/syllable/animation/float.cpp
	render/components/line/normal/main/syllable/animation/mask.cpp
	render/components/line/interlude/index.cpp
	render/core/effect.cpp
	render/core/layout.cpp
	render/core/line.cpp
	render/core/scroll.cpp
	render/core/transition.cpp
	render/renderer.cpp
)
add_library(music_lyric_player::render ALIAS music_lyric_player_render)

target_compile_features(music_lyric_player_render PUBLIC cxx_std_20)

target_include_directories(music_lyric_player_render PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(music_lyric_player_render PUBLIC
	music_lyric_player::playback
	music_lyric::model
	skia::skia)

add_library(music_lyric_player_backend
	backend/gpu/vulkan.cpp
)
add_library(music_lyric_player::backend ALIAS music_lyric_player_backend)

target_compile_features(music_lyric_player_backend PUBLIC cxx_std_20)

target_include_directories(music_lyric_player_backend PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(music_lyric_player_backend PUBLIC
	skia::skia
	Vulkan::Vulkan)
