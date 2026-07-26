#ifndef MUSIC_LYRIC_PLAYER_RENDERING_UTILS_FRAGMENT_BUILDER_H_
#define MUSIC_LYRIC_PLAYER_RENDERING_UTILS_FRAGMENT_BUILDER_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

#include "include/core/SkPoint.h"
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
			start                          = line.runs.front().utf8Begin;
			const shaping::ShapedRun& last = line.runs.back();
			end                            = last.utf8Begin + last.utf8Size;
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
		float             maxWidth  = 0.0f;
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
		group.advance                   = maxWidth;
		group.ascent                    = last.ascent;
		group.descent                   = last.descent;
		group.height                    = last.ascent + last.descent;
		group.bounds                    = detail::typographicBounds(group.advance, group.height);

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

	/**
	 * Splits a shaped text into per-cluster fragment groups for the emphasize per-character animation.
	 * Glyphs keep their absolute in-word positions and every origin stays {0,0}, so painting all groups at the word origin issues the same draw parameters as the single-blob path.
	 * Each group's bounds record the cluster's x start and advance inside the word box, which the animation uses to find the cell center.
	 * Spaces form their own clusters and RTL runs group in visual order, mirroring the web per-character split without re-shaping.
	 */
	inline std::vector<FragmentGroup> makeClusterGroups(const shaping::ShapedText& text, const char* utf8) {
		std::vector<FragmentGroup> groups;
		for (const shaping::ShapedLine& line : text.lines) {
			// One pending cell per cluster boundary, collected in visual order across the line's runs.
			struct Cell {
				const shaping::ShapedRun* run;
				std::size_t               first;
				std::size_t               count;
				uint32_t                  cluster;
				float                     xStart;
			};
			std::vector<Cell> cells;
			for (const shaping::ShapedRun& run : line.runs) {
				for (std::size_t i = 0; i < run.glyphs.size(); ++i) {
					const shaping::ShapedGlyph& glyph = run.glyphs[i];
					if (cells.empty() || cells.back().run != &run || cells.back().cluster != glyph.cluster) {
						cells.push_back(Cell{&run, i, 0, glyph.cluster, glyph.position.fX});
					}
					++cells.back().count;
				}
			}

			const float height = line.ascent + line.descent;
			for (std::size_t c = 0; c < cells.size(); ++c) {
				const Cell& cell    = cells[c];
				const float nextX   = c + 1 < cells.size() ? cells[c + 1].xStart : line.width;
				const float advance = std::max(nextX - cell.xStart, 0.0f);

				// The cluster's utf8 range ends at the run's next larger cluster offset, or at the run end for the run's logically last cluster.
				std::size_t textEnd = cell.run->utf8Begin + cell.run->utf8Size;
				for (const shaping::ShapedGlyph& other : cell.run->glyphs) {
					if (other.cluster > cell.cluster && other.cluster < textEnd) {
						textEnd = other.cluster;
					}
				}

				FragmentGroup group;
				group.advance = advance;
				group.ascent  = line.ascent;
				group.descent = line.descent;
				group.height  = height;
				group.bounds  = SkRect::MakeXYWH(cell.xStart, 0.0f, advance, height);

				SkTextBlobBuilder builder;
				const int         glyphCount = static_cast<int>(cell.count);
				const int         textCount  = static_cast<int>(textEnd - cell.cluster);

				const SkTextBlobBuilder::RunBuffer& buffer = builder.allocRunTextPos(cell.run->font, glyphCount, textCount);
				if (buffer.utf8text && utf8) {
					std::memcpy(buffer.utf8text, utf8 + cell.cluster, static_cast<std::size_t>(textCount));
				}
				SkPoint* points = buffer.points();
				for (std::size_t i = 0; i < cell.count; ++i) {
					const shaping::ShapedGlyph& glyph = cell.run->glyphs[cell.first + i];
					buffer.glyphs[i]                  = glyph.glyph;
					points[i]                         = glyph.position;
					buffer.clusters[i]                = glyph.cluster - cell.cluster;
				}

				GlyphFragment fragment;
				fragment.blob      = builder.make();
				fragment.origin    = {0.0f, 0.0f};
				fragment.bounds    = group.bounds;
				fragment.advance   = advance;
				fragment.textStart = cell.cluster;
				fragment.textEnd   = textEnd;
				group.fragments.push_back(std::move(fragment));
				groups.push_back(std::move(group));
			}
		}
		return groups;
	}
} // namespace music_lyric_player::rendering::utils::fragment

#endif // MUSIC_LYRIC_PLAYER_RENDERING_UTILS_FRAGMENT_BUILDER_H_
