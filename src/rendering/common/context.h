#ifndef MUSIC_LYRIC_PLAYER_RENDERING_COMMON_CONTEXT_H_
#define MUSIC_LYRIC_PLAYER_RENDERING_COMMON_CONTEXT_H_

#include "include/core/SkRefCnt.h"
#include "rendering/config/index.h"

class SkUnicode;
class SkFontMgr;
class SkShaper;

namespace music_lyric_player::rendering::common {
	/**
	 * Non-owning bundle of shared resources handed to line components each frame for layout and paint.
	 * Carries the resolved config plus the font and unicode backends so a component builds its own paragraph.
	 * Carries playback time so content-timed animations remain stable across pause and seek.
	 */
	struct RenderContext {
		const config::Root&     config;
		const sk_sp<SkUnicode>& unicode;
		const sk_sp<SkFontMgr>& fontMgr;
		SkShaper*               shaper;
		double                  currentTime;
	};
} // namespace music_lyric_player::rendering::common

#endif // MUSIC_LYRIC_PLAYER_RENDERING_COMMON_CONTEXT_H_
