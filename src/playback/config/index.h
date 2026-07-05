#ifndef MUSIC_LYRIC_PLAYER_PLAYBACK_CONFIG_INDEX_H_
#define MUSIC_LYRIC_PLAYER_PLAYBACK_CONFIG_INDEX_H_

#include "playback/config/config.gen.h"
#include "utils/manager/config.h"

namespace music_lyric_player::playback::config {
	/**
	 * Config store for the timing engine, bound to the root Root / RootPatch.
	 */
	using Manager = ::music_lyric_player::utils::config::Manager<Root, RootPatch>;

	/**
	 * Dot-path constants addressing config fields, e.g. `Keys::offset::global`; pass these to `keyHas`.
	 */
	namespace Keys = ::music_lyric_player::playback::config::RootKeys;

	/**
	 * Set of changed dot-path keys emitted on each config update; the argument to `keyHas`.
	 */
	using ::music_lyric_player::utils::config::ChangeKeys;

	/**
	 * Whether an exact dot-path key changed in a key set.
	 */
	using ::music_lyric_player::utils::config::keyHas;
} // namespace music_lyric_player::playback::config

#endif // MUSIC_LYRIC_PLAYER_PLAYBACK_CONFIG_INDEX_H_
