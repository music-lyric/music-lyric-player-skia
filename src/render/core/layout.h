#ifndef MUSIC_LYRIC_PLAYER_RENDER_CORE_LAYOUT_H_
#define MUSIC_LYRIC_PLAYER_RENDER_CORE_LAYOUT_H_

#include <cstddef>
#include <memory>
#include <vector>

#include "render/components/line/base/index.h"

namespace music_lyric_player::render::core {
	/**
	 * Stacks line components vertically, giving each a top offset of the previous line's plus the gap.
	 * Owns positions only; paragraph shaping stays in the components.
	 */
	class LayoutManager {
	public:
		/**
		 * Recomputes every line's top by stacking measured heights separated by `gap`.
		 */
		void update(const std::vector<std::unique_ptr<components::line::base::Element>>& lines, float gap);

		/**
		 * The top offset of line `index` in logical pixels; valid after `update`.
		 */
		float top(std::size_t index) const {
			return tops_[index];
		}

	private:
		std::vector<float> tops_;
	};
} // namespace music_lyric_player::render::core

#endif // MUSIC_LYRIC_PLAYER_RENDER_CORE_LAYOUT_H_
