#ifndef MUSIC_LYRIC_PLAYER_RENDERING_UTILS_LAYOUT_INLINE_FLOW_H_
#define MUSIC_LYRIC_PLAYER_RENDERING_UTILS_LAYOUT_INLINE_FLOW_H_

#include <algorithm>
#include <cstddef>
#include <vector>

#include "rendering/config/layout/index.gen.h"

namespace music_lyric_player::rendering::utils::layout {
	/**
	 * The intrinsic metrics of one inline cell handed to the greedy row packer.
	 * `gap` is the spacing kept before the cell and is dropped whenever the cell falls at the start of a row.
	 */
	struct InlineCell {
		float advance = 0.0f;
		float gap     = 0.0f;
		float ascent  = 0.0f;
		float descent = 0.0f;
	};

	/**
	 * The final line-relative position of one packed cell, baseline-aligned inside its wrapped row.
	 */
	struct InlinePlacement {
		float x = 0.0f;
		float y = 0.0f;
	};

	/**
	 * The result of packing inline cells: one placement per input cell plus the summed row height.
	 */
	struct InlineFlowLayout {
		std::vector<InlinePlacement> placements;
		float                        height = 0.0f;
	};

	namespace detail {
		/**
		 * Computes the horizontal offset that shifts a packed row of the given width inside the block.
		 */
		inline float rowAlignOffset(config::layout::Align align, float blockWidth, float rowWidth) {
			switch (align) {
			case config::layout::Align::Center:
				return (blockWidth - rowWidth) * 0.5f;
			case config::layout::Align::Right:
				return blockWidth - rowWidth;
			case config::layout::Align::Left:
			default:
				return 0.0f;
			}
		}
	} // namespace detail

	/**
	 * Greedily packs inline cells into rows bounded by `width`, aligns each row, and shares one baseline per row.
	 * A row's first cell drops its leading gap, the row baseline sits at the row's max ascent, and each cell hangs from it.
	 * The returned placements map one-to-one onto `cells`; empty input yields an empty result with zero height.
	 */
	inline InlineFlowLayout layoutInlineFlow(const std::vector<InlineCell>& cells, float width, config::layout::Align align) {
		InlineFlowLayout result;
		result.placements.resize(cells.size());

		struct Row {
			std::size_t begin   = 0;
			std::size_t end     = 0;
			float       width   = 0.0f;
			float       ascent  = 0.0f;
			float       descent = 0.0f;
		};

		std::vector<Row> rows;
		Row              row;
		for (std::size_t i = 0; i < cells.size(); ++i) {
			const InlineCell& cell       = cells[i];
			const bool        atRowStart = row.begin == row.end;
			float             gap        = atRowStart ? 0.0f : cell.gap;
			if (!atRowStart && row.width + gap + cell.advance > width) {
				rows.push_back(row);
				row       = Row{};
				row.begin = i;
				row.end   = i;
				gap       = 0.0f;
			}

			result.placements[i].x = row.width + gap;
			row.width += gap + cell.advance;
			row.ascent  = std::max(row.ascent, cell.ascent);
			row.descent = std::max(row.descent, cell.descent);
			row.end     = i + 1;
		}
		if (row.begin != row.end) {
			rows.push_back(row);
		}

		float rowY = 0.0f;
		for (const Row& current : rows) {
			const float offsetX = detail::rowAlignOffset(align, width, current.width);
			for (std::size_t i = current.begin; i < current.end; ++i) {
				result.placements[i].x += offsetX;
				result.placements[i].y = rowY + current.ascent - cells[i].ascent;
			}
			rowY += current.ascent + current.descent;
		}
		result.height = rowY;
		return result;
	}
} // namespace music_lyric_player::rendering::utils::layout

#endif // MUSIC_LYRIC_PLAYER_RENDERING_UTILS_LAYOUT_INLINE_FLOW_H_
