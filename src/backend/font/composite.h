#ifndef MUSIC_LYRIC_PLAYER_BACKEND_FONT_COMPOSITE_H_
#define MUSIC_LYRIC_PLAYER_BACKEND_FONT_COMPOSITE_H_

#include <vector>

#include "include/core/SkRefCnt.h"

class SkFontMgr;

namespace music_lyric_player::backend::font {
	/**
	 * Creates a font manager answering from `layers` in order, dropping the null ones.
	 * A name lookup takes the first layer carrying that family, so an upper layer shadows a family of the same name below it.
	 * The per-character fallback asks every layer instead, because a character the upper layers cannot draw may still be covered further down.
	 * A single layer is handed back as it stands rather than wrapped, and an empty font manager comes back when no layer is left.
	 * Every layer has to be immutable, as Skia's own managers are, since the family counts are read once when the composite is built.
	 */
	sk_sp<SkFontMgr> createCompositeFontMgr(std::vector<sk_sp<SkFontMgr>> layers);
} // namespace music_lyric_player::backend::font

#endif // MUSIC_LYRIC_PLAYER_BACKEND_FONT_COMPOSITE_H_
