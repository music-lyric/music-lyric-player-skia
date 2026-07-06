#ifndef MUSIC_LYRIC_PLAYER_RENDER_COMMON_CONTEXT_H_
#define MUSIC_LYRIC_PLAYER_RENDER_COMMON_CONTEXT_H_

#include "include/core/SkRefCnt.h"
#include "render/config/index.h"

class SkUnicode;

namespace skia::textlayout {
	class FontCollection;
} // namespace skia::textlayout

namespace music_lyric_player::render::common {
	/**
	 * Non-owning bundle of shared resources handed to line components each frame for layout and paint.
	 * Carries the resolved config plus the font and unicode backends so a component builds its own paragraph.
	 */
	struct RenderContext {
		const config::Root&                              config;
		const sk_sp<::skia::textlayout::FontCollection>& fonts;
		const sk_sp<SkUnicode>&                          unicode;
	};
} // namespace music_lyric_player::render::common

#endif // MUSIC_LYRIC_PLAYER_RENDER_COMMON_CONTEXT_H_
