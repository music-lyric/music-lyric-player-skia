#ifndef MUSIC_LYRIC_PLAYER_RENDER_CONFIG_CONFIG_H_
#define MUSIC_LYRIC_PLAYER_RENDER_CONFIG_CONFIG_H_

#include "render/config/config.gen.h"
#include "utils/manager/config.h"

namespace music_lyric_player::render {
	/**
	 * Config store for the renderer, bound to this layer's generated `Config` / `ConfigPatch`.
	 */
	using ConfigManager = ::music_lyric_player::ConfigManager<Config, ConfigPatch>;
} // namespace music_lyric_player::render

#endif
