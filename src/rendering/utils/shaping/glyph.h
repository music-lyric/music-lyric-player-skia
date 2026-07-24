#ifndef MUSIC_LYRIC_PLAYER_RENDERING_UTILS_SHAPING_GLYPH_H_
#define MUSIC_LYRIC_PLAYER_RENDERING_UTILS_SHAPING_GLYPH_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "include/core/SkFont.h"
#include "include/core/SkPoint.h"
#include "include/core/SkTypes.h"

namespace music_lyric_player::rendering::utils::shaping {
	/**
	 * One positioned glyph: its id, its absolute position in the shaped block, and the utf8 cluster it came from.
	 * The position already carries the run origin and baseline, so a blob can be rebuilt from it verbatim.
	 * The cluster is an absolute byte offset into the shaped string; a per-run rebase happens only at blob build time.
	 */
	struct ShapedGlyph {
		SkGlyphID glyph    = 0;
		SkPoint   position = {0.0f, 0.0f};
		uint32_t  cluster  = 0;
	};

	/**
	 * One shaping run: a maximal glyph span sharing a font, tagged with the utf8 range it covers.
	 * The range lets a consumer copy the run's cluster text and rebase the absolute clusters back to run-local.
	 */
	struct ShapedRun {
		SkFont                   font;
		std::size_t              utf8Begin = 0;
		std::size_t              utf8Size  = 0;
		std::vector<ShapedGlyph> glyphs;
	};

	/**
	 * One shaped line: its runs plus the metrics layout needs — top-to-baseline ascent, baseline-to-bottom descent, advance width.
	 * Ascent and descent are positive distances; a multi-line result stacks lines top-down using their sum.
	 */
	struct ShapedLine {
		std::vector<ShapedRun> runs;
		float                  ascent  = 0.0f;
		float                  descent = 0.0f;
		float                  width   = 0.0f;
	};

	/**
	 * A whole shaped text: one line when shaped unbounded, or several when width-wrapped.
	 * Glyph positions are absolute across the block, so line N already sits below lines 0..N-1.
	 */
	struct ShapedText {
		std::vector<ShapedLine> lines;
	};
} // namespace music_lyric_player::rendering::utils::shaping

#endif // MUSIC_LYRIC_PLAYER_RENDERING_UTILS_SHAPING_GLYPH_H_
