#ifndef MUSIC_LYRIC_PLAYER_RENDERING_UTILS_FRAGMENT_GLYPH_H_
#define MUSIC_LYRIC_PLAYER_RENDERING_UTILS_FRAGMENT_GLYPH_H_

#include <cstddef>
#include <cstdint>

#include "include/core/SkPoint.h"
#include "include/core/SkRect.h"
#include "include/core/SkRefCnt.h"
#include "include/core/SkTextBlob.h"

namespace music_lyric_player::rendering::utils::fragment {
	/**
	 * One paintable glyph span: a cached blob plus its group-relative origin, layout advance, and source range.
	 * The blob is an internal paint cache; consumers paint through FragmentGroup and never rebuild it.
	 * Bounds are the typographic box in Phase 2 (not true ink), so mask layers stay bit-identical to today.
	 * bidiLevel is stubbed to zero until a later phase records direction from the shaper.
	 */
	struct GlyphFragment {
		sk_sp<SkTextBlob> blob;
		SkPoint           origin    = {0.0f, 0.0f};
		SkRect            bounds    = SkRect::MakeEmpty();
		float             advance   = 0.0f;
		std::size_t       textStart = 0;
		std::size_t       textEnd   = 0;
		std::uint8_t      bidiLevel = 0;
	};
} // namespace music_lyric_player::rendering::utils::fragment

#endif // MUSIC_LYRIC_PLAYER_RENDERING_UTILS_FRAGMENT_GLYPH_H_
