#ifndef MUSIC_LYRIC_PLAYER_RENDERING_UTILS_SHAPING_FONT_H_
#define MUSIC_LYRIC_PLAYER_RENDERING_UTILS_SHAPING_FONT_H_

#include <string>

#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkFontTypes.h"
#include "include/core/SkRefCnt.h"
#include "include/core/SkScalar.h"
#include "include/core/SkTypeface.h"

namespace music_lyric_player::rendering::utils::shaping {
	/**
	 * Builds a body-text font with baseline snapping disabled so a moving glyph stays sub-pixel smooth.
	 * Per-glyph fallback keeps the same flags, so minority-script glyphs also escape the vertical pixel grid.
	 */
	inline SkFont buildBodyFont(const sk_sp<SkFontMgr>& fontMgr, const std::string& family, float size) {
		sk_sp<SkTypeface> typeface;
		if (!family.empty()) {
			typeface = fontMgr->matchFamilyStyle(family.c_str(), SkFontStyle::Normal());
		}
		if (!typeface) {
			typeface = fontMgr->matchFamilyStyle(nullptr, SkFontStyle::Normal());
		}

		SkFont font(typeface, static_cast<SkScalar>(size));
		font.setSubpixel(true);
		font.setBaselineSnap(false);
		font.setEdging(SkFont::Edging::kAntiAlias);
		font.setHinting(SkFontHinting::kNone);
		return font;
	}
} // namespace music_lyric_player::rendering::utils::shaping

#endif // MUSIC_LYRIC_PLAYER_RENDERING_UTILS_SHAPING_FONT_H_
