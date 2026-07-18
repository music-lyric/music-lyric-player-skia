#ifndef MUSIC_LYRIC_PLAYER_RENDERING_CORE_LINE_H_
#define MUSIC_LYRIC_PLAYER_RENDERING_CORE_LINE_H_

#include <cstddef>
#include <memory>
#include <vector>

#include "music_lyric_model.h"
#include "rendering/components/line/base/index.h"

namespace music_lyric_player::rendering::common {
	struct RenderContext;
} // namespace music_lyric_player::rendering::common

namespace music_lyric_player::rendering::core {
	/**
	 * Owns the lyric line components and their lifecycle: builds one per lyric line and re-wraps them on demand.
	 * Holds the collection only; vertical placement lives in `LayoutManager` and painting in the components.
	 */
	class LineManager {
	public:
		/**
		 * Rebuilds the line list from a freshly loaded lyric, dropping the previous lines.
		 * Marks the layout dirty so the new lines wrap on the next frame.
		 */
		void rebuild(const music_lyric_model::parsed::Info& info);

		/**
		 * Drops all lines and marks the layout dirty.
		 */
		void clear();

		/**
		 * Marks the layout dirty so a config or width change re-wraps every line on the next frame.
		 */
		void invalidateLayout();

		/**
		 * (Re)lays out every line for `contentWidth` when the layout is dirty or the width changed.
		 */
		void ensureLayout(float contentWidth, const common::RenderContext& context);

		/**
		 * The line components in lyric order; valid for stacking and painting after `ensureLayout`.
		 */
		const std::vector<std::unique_ptr<components::line::base::Element>>& all() const {
			return this->lines;
		}

		/**
		 * Whether there are no lines.
		 */
		bool empty() const {
			return this->lines.empty();
		}

		/**
		 * The number of lines.
		 */
		std::size_t size() const {
			return this->lines.size();
		}

		/**
		 * The line component at `index`; valid for `index` below `size()`.
		 */
		const components::line::base::Element& at(std::size_t index) const {
			return *this->lines[index];
		}

	private:
		std::vector<std::unique_ptr<components::line::base::Element>> lines;
		bool                                                          layoutDirty = true;
		float                                                         layoutWidth = -1.0f; // content width the lines were wrapped to
	};
} // namespace music_lyric_player::rendering::core

#endif // MUSIC_LYRIC_PLAYER_RENDERING_CORE_LINE_H_
