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
	 * Builds a body-text font; `subpixel` frees glyph placement from the pixel grid for text that animates.
	 * Static text passes false so every glyph rounds to a whole device pixel and its stroke edges stay crisp; free two-axis sub-pixel phases smear them instead.
	 * Normal hinting grid-fits the outlines the way DirectWrite does natively without touching metrics, so the layout is identical either way.
	 * Per-glyph fallback keeps the same flags, so minority-script glyphs render like the primary ones.
	 */
	inline SkFont buildBodyFont(const sk_sp<SkFontMgr>& fontMgr, const std::string& family, float size, bool subpixel) {
		sk_sp<SkTypeface> typeface;
		if (!family.empty()) {
			typeface = fontMgr->matchFamilyStyle(family.c_str(), SkFontStyle::Normal());
		}
		if (!typeface) {
			typeface = fontMgr->matchFamilyStyle(nullptr, SkFontStyle::Normal());
		}
		// A font manager built from registered data has no platform default, so the null family above resolves to nothing.
		// The first family it holds stands in for one.
		// A system font stack resolves the null family itself and never reaches here.
		if (!typeface && fontMgr->countFamilies() > 0) {
			const sk_sp<SkFontStyleSet> fallback = fontMgr->createStyleSet(0);
			if (fallback != nullptr) {
				typeface = fallback->matchStyle(SkFontStyle::Normal());
			}
		}

		SkFont font(typeface, static_cast<SkScalar>(size));
		font.setSubpixel(subpixel);
		font.setBaselineSnap(!subpixel);
		font.setEdging(SkFont::Edging::kAntiAlias);
		font.setHinting(SkFontHinting::kNormal);
		return font;
	}
} // namespace music_lyric_player::rendering::utils::shaping

#endif // MUSIC_LYRIC_PLAYER_RENDERING_UTILS_SHAPING_FONT_H_
