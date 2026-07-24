#ifndef MUSIC_LYRIC_PLAYER_RENDERING_UTILS_FRAGMENT_BUILDER_H_
#define MUSIC_LYRIC_PLAYER_RENDERING_UTILS_FRAGMENT_BUILDER_H_

#include <algorithm>
#include <cstddef>
#include <utility>

#include "include/core/SkRect.h"
#include "include/core/SkTextBlob.h"

#include "rendering/utils/fragment/glyph.h"
#include "rendering/utils/fragment/group.h"
#include "rendering/utils/shaping/glyph.h"
#include "rendering/utils/shaping/shaper.h"

namespace music_lyric_player::rendering::utils::fragment {
	namespace detail {
		/**
		 * Builds the typographic box used as Phase-2 visual bounds (not true glyph ink).
		 * Matching today's measured width × height keeps mask saveLayer sizes bit-identical.
		 */
		inline SkRect typographicBounds(float advance, float height) {
			return SkRect::MakeXYWH(0.0f, 0.0f, advance, height);
		}

		/**
		 * Returns the absolute utf8 range covered by a shaped line's runs, or {0,0} when empty.
		 */
		inline void lineTextRange(const shaping::ShapedLine& line, std::size_t& start, std::size_t& end) {
			start = 0;
			end   = 0;
			if (line.runs.empty()) {
				return;
			}
			start = line.runs.front().utf8Begin;
			const shaping::ShapedRun& last = line.runs.back();
			end                           = last.utf8Begin + last.utf8Size;
		}
	} // namespace detail

	/**
	 * Builds one fragment group from a single shaped line (one blob via shaping::appendLine).
	 * Origin defaults to {0,0}; plain layout may later set origin.x to the resolved alignment offset.
	 */
	inline FragmentGroup makeLineGroup(const shaping::ShapedLine& line, const char* utf8) {
		FragmentGroup group;
		group.advance = line.width;
		group.ascent  = line.ascent;
		group.descent = line.descent;
		group.height  = line.ascent + line.descent;
		group.bounds  = detail::typographicBounds(group.advance, group.height);

		SkTextBlobBuilder builder;
		shaping::appendLine(builder, line, utf8);

		GlyphFragment fragment;
		fragment.blob    = builder.make();
		fragment.origin  = {0.0f, 0.0f};
		fragment.bounds  = group.bounds;
		fragment.advance = group.advance;
		detail::lineTextRange(line, fragment.textStart, fragment.textEnd);
		group.fragments.push_back(std::move(fragment));
		return group;
	}

	/**
	 * Builds one fragment group from a whole ShapedText by concatenating every line into a single blob.
	 * Matches today's Word path: advance is the max line width, metrics come from the last line.
	 * Returns an empty group when the shaped text has no lines.
	 */
	inline FragmentGroup makeTextGroup(const shaping::ShapedText& text, const char* utf8) {
		if (text.lines.empty()) {
			return {};
		}

		FragmentGroup     group;
		SkTextBlobBuilder builder;
		float             maxWidth = 0.0f;
		std::size_t       textStart = 0;
		std::size_t       textEnd   = 0;
		bool              rangeSet  = false;

		for (const shaping::ShapedLine& line : text.lines) {
			shaping::appendLine(builder, line, utf8);
			maxWidth = std::max(maxWidth, line.width);

			std::size_t lineStart = 0;
			std::size_t lineEnd   = 0;
			detail::lineTextRange(line, lineStart, lineEnd);
			if (!rangeSet) {
				textStart = lineStart;
				textEnd   = lineEnd;
				rangeSet  = true;
			} else {
				textStart = std::min(textStart, lineStart);
				textEnd   = std::max(textEnd, lineEnd);
			}
		}

		const shaping::ShapedLine& last = text.lines.back();
		group.advance = maxWidth;
		group.ascent  = last.ascent;
		group.descent = last.descent;
		group.height  = last.ascent + last.descent;
		group.bounds  = detail::typographicBounds(group.advance, group.height);

		GlyphFragment fragment;
		fragment.blob      = builder.make();
		fragment.origin    = {0.0f, 0.0f};
		fragment.bounds    = group.bounds;
		fragment.advance   = group.advance;
		fragment.textStart = textStart;
		fragment.textEnd   = textEnd;
		group.fragments.push_back(std::move(fragment));
		return group;
	}
} // namespace music_lyric_player::rendering::utils::fragment

#endif // MUSIC_LYRIC_PLAYER_RENDERING_UTILS_FRAGMENT_BUILDER_H_
