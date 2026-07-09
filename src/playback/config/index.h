#ifndef MUSIC_LYRIC_PLAYER_PLAYBACK_CONFIG_INDEX_H_
#define MUSIC_LYRIC_PLAYER_PLAYBACK_CONFIG_INDEX_H_

#include "playback/config/config.gen.h"
#include "utils/manager/config.h"

namespace music_lyric_player::playback::config {
	/**
	 * Config store for the timing engine, bound to the root Root / RootPatch / RootChange.
	 */
	using Manager = ::music_lyric_player::utils::config::Manager<Root, RootPatch, RootChange>;
} // namespace music_lyric_player::playback::config

#endif // MUSIC_LYRIC_PLAYER_PLAYBACK_CONFIG_INDEX_H_
