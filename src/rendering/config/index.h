#ifndef MUSIC_LYRIC_PLAYER_RENDERING_CONFIG_INDEX_H_
#define MUSIC_LYRIC_PLAYER_RENDERING_CONFIG_INDEX_H_

#include "rendering/config/config.gen.h"
#include "rendering/config/container/index.gen.h"
#include "rendering/config/layout/index.gen.h"
#include "rendering/config/line/index.gen.h"
#include "rendering/config/scroll/index.gen.h"
#include "utils/manager/config.h"

namespace music_lyric_player::rendering::config {
	/**
	 * Config store for the renderer, bound to the root Root / RootPatch / RootChange.
	 */
	using Manager = ::music_lyric_player::utils::config::Manager<Root, RootPatch, RootChange>;
} // namespace music_lyric_player::rendering::config

#endif // MUSIC_LYRIC_PLAYER_RENDERING_CONFIG_INDEX_H_
