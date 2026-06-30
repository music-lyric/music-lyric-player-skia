#ifndef MUSIC_LYRIC_PLAYER_BASE_CONFIG_CONFIG_H_
#define MUSIC_LYRIC_PLAYER_BASE_CONFIG_CONFIG_H_

#include "base/config/config.gen.h"
#include "utils/manager/config.h"

namespace music_lyric_player::base {
	/**
	 * Config store for the timing engine, bound to this layer's generated `Config` / `ConfigPatch`.
	 */
	using ConfigManager = ::music_lyric_player::ConfigManager<Config, ConfigPatch>;
} // namespace music_lyric_player::base

#endif
