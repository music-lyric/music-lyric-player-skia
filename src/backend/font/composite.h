#ifndef MUSIC_LYRIC_PLAYER_BACKEND_FONT_COMPOSITE_H_
#define MUSIC_LYRIC_PLAYER_BACKEND_FONT_COMPOSITE_H_

#include "include/core/SkRefCnt.h"

class SkFontMgr;

namespace music_lyric_player::backend::font {
	/**
	 * Creates a font manager that answers from `overlay` first and falls through to `base`, or returns whichever one is given when the other is null.
	 * Families enumerate as the overlay's followed by the base's, and a name the overlay does not carry is resolved by the base, so registering a font shadows a system family of the same name without hiding the rest of the stack.
	 * The result holds a reference to both and is immutable, exactly like the managers it composes.
	 */
	sk_sp<SkFontMgr> createCompositeFontMgr(sk_sp<SkFontMgr> overlay, sk_sp<SkFontMgr> base);
} // namespace music_lyric_player::backend::font

#endif // MUSIC_LYRIC_PLAYER_BACKEND_FONT_COMPOSITE_H_
