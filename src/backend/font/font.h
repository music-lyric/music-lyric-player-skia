#ifndef MUSIC_LYRIC_PLAYER_BACKEND_FONT_FONT_H_
#define MUSIC_LYRIC_PLAYER_BACKEND_FONT_FONT_H_

#include "include/core/SkRefCnt.h"

class SkFontMgr;

namespace music_lyric_player::backend::font {
	/**
	 * Creates the platform's system font manager, which resolves families from the OS font stack.
	 */
	sk_sp<SkFontMgr> createFontMgr();
} // namespace music_lyric_player::backend::font

#endif // MUSIC_LYRIC_PLAYER_BACKEND_FONT_FONT_H_
