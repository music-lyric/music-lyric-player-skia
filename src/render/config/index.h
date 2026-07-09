#ifndef MUSIC_LYRIC_PLAYER_RENDER_CONFIG_INDEX_H_
#define MUSIC_LYRIC_PLAYER_RENDER_CONFIG_INDEX_H_

#include "render/config/config.gen.h"
#include "render/config/container/index.gen.h"
#include "render/config/layout/index.gen.h"
#include "render/config/line/index.gen.h"
#include "render/config/scroll/index.gen.h"
#include "utils/manager/config.h"

namespace music_lyric_player::render::config {
	/**
	 * Config store for the renderer, bound to the root Root / RootPatch / RootChange.
	 */
	using Manager = ::music_lyric_player::utils::config::Manager<Root, RootPatch, RootChange>;
} // namespace music_lyric_player::render::config

#endif // MUSIC_LYRIC_PLAYER_RENDER_CONFIG_INDEX_H_
