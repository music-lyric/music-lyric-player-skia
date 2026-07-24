#ifndef MUSIC_LYRIC_PLAYER_RENDERING_UTILS_FRAGMENT_GROUP_H_
#define MUSIC_LYRIC_PLAYER_RENDERING_UTILS_FRAGMENT_GROUP_H_

#include <vector>

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkPaint.h"
#include "include/core/SkRect.h"

#include "rendering/utils/fragment/glyph.h"

namespace music_lyric_player::rendering::utils::fragment {
	/**
	 * A logical paint unit: one or more glyph fragments sharing layout metrics and a single paint entry.
	 * Advance is the layout width; bounds is the visual box used for clipping and masks — never reuse one for both.
	 * Ascent, descent, and height describe the typographic stack the fragments were built from.
	 */
	struct FragmentGroup {
		std::vector<GlyphFragment> fragments;
		SkRect                     bounds;
		float                      advance = 0.0f;
		float                      ascent  = 0.0f;
		float                      descent = 0.0f;
		float                      height  = 0.0f;

		/**
		 * Reports whether the group holds at least one paintable fragment.
		 */
		explicit operator bool() const {
			return !this->fragments.empty();
		}

		/**
		 * Paints every fragment's blob at (x, y) plus the fragment's group-relative origin.
		 */
		void paint(SkCanvas* canvas, float x, float y, SkColor color) const {
			if (!canvas || this->fragments.empty()) {
				return;
			}
			SkPaint paint;
			paint.setAntiAlias(true);
			paint.setColor(color);
			for (const GlyphFragment& fragment : this->fragments) {
				if (!fragment.blob) {
					continue;
				}
				canvas->drawTextBlob(fragment.blob, x + fragment.origin.fX, y + fragment.origin.fY, paint);
			}
		}
	};
} // namespace music_lyric_player::rendering::utils::fragment

#endif // MUSIC_LYRIC_PLAYER_RENDERING_UTILS_FRAGMENT_GROUP_H_
