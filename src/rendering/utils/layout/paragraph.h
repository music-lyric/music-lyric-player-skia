#ifndef MUSIC_LYRIC_PLAYER_RENDERING_UTILS_LAYOUT_PARAGRAPH_H_
#define MUSIC_LYRIC_PLAYER_RENDERING_UTILS_LAYOUT_PARAGRAPH_H_

#include <algorithm>
#include <cstddef>
#include <utility>

#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkRect.h"
#include "include/core/SkRefCnt.h"
#include "modules/skshaper/include/SkShaper.h"
#include "modules/skunicode/include/SkUnicode.h"

#include "rendering/config/layout/index.gen.h"
#include "rendering/utils/fragment/builder.h"
#include "rendering/utils/fragment/glyph.h"
#include "rendering/utils/fragment/group.h"
#include "rendering/utils/shaping/glyph.h"
#include "rendering/utils/shaping/shaper.h"

namespace music_lyric_player::rendering::utils::layout {
	/**
	 * The result of laying out one plain paragraph: its paint group plus the block height.
	 * The group advance is the widest wrapped line; height is the summed per-line ascent and descent.
	 */
	struct ParagraphLayout {
		fragment::FragmentGroup group;
		float                   height = 0.0f;

		/**
		 * Reports whether the paragraph produced at least one paintable fragment.
		 */
		explicit operator bool() const {
			return static_cast<bool>(this->group);
		}
	};

	namespace detail {
		/**
		 * Computes the horizontal offset that shifts a shaped line of the given width inside the block.
		 */
		inline float alignOffset(config::layout::Align align, float blockWidth, float lineWidth) {
			switch (align) {
			case config::layout::Align::Center:
				return (blockWidth - lineWidth) * 0.5f;
			case config::layout::Align::Right:
				return blockWidth - lineWidth;
			case config::layout::Align::Left:
			default:
				return 0.0f;
			}
		}
	} // namespace detail

	/**
	 * Shapes, wraps, aligns, and packs a utf8 paragraph into a single fragment group with one fragment per wrapped line.
	 * The width bounds shaper-driven wrapping and is the block width each line's alignment offset is measured against.
	 * Empty utf8 or failed shaping yields an empty result, so a caller can keep its own pre-layout early return.
	 */
	inline ParagraphLayout layoutParagraph(SkShaper& shaper, const sk_sp<SkUnicode>& unicode, const sk_sp<SkFontMgr>& fontMgr, const SkFont& font, const char* utf8, std::size_t bytes, float width, config::layout::Align align) {
		ParagraphLayout result;

		const shaping::ShapedText shaped = shaping::shapeText(shaper, unicode, fontMgr, font, utf8, bytes, width);

		// One fragment per wrapped line; the alignment offset is baked into origin.x so paint stays a single group call.
		result.group.fragments.reserve(shaped.lines.size());
		float maxWidth = 0.0f;
		for (const shaping::ShapedLine& line : shaped.lines) {
			fragment::FragmentGroup lineGroup = fragment::makeLineGroup(line, utf8);
			if (lineGroup.fragments.empty()) {
				continue;
			}
			fragment::GlyphFragment fragment = std::move(lineGroup.fragments.front());
			fragment.origin.fX               = detail::alignOffset(align, width, line.width);
			result.group.fragments.push_back(std::move(fragment));
			maxWidth = std::max(maxWidth, line.width);
			result.height += line.ascent + line.descent;
		}
		result.group.advance = maxWidth;
		result.group.height  = result.height;
		result.group.bounds  = SkRect::MakeXYWH(0.0f, 0.0f, maxWidth, result.height);
		return result;
	}
} // namespace music_lyric_player::rendering::utils::layout

#endif // MUSIC_LYRIC_PLAYER_RENDERING_UTILS_LAYOUT_PARAGRAPH_H_
