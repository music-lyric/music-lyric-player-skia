add_library(music_lyric_player_base
	utils/steady_clock.cpp
	base/merger.cpp
	base/offset.cpp
	base/player.cpp
)
add_library(music_lyric_player::base ALIAS music_lyric_player_base)

target_compile_features(music_lyric_player_base PUBLIC cxx_std_20)

target_include_directories(music_lyric_player_base PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(music_lyric_player_base PUBLIC music_lyric_model::music_lyric_model)
