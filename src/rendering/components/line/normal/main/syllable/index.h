#ifndef MUSIC_LYRIC_PLAYER_RENDERING_COMPONENTS_LINE_NORMAL_MAIN_SYLLABLE_INDEX_H_
#define MUSIC_LYRIC_PLAYER_RENDERING_COMPONENTS_LINE_NORMAL_MAIN_SYLLABLE_INDEX_H_

#include <memory>
#include <vector>

#include "rendering/components/line/normal/main/syllable/animation/mask.h"

class SkCanvas;

namespace lyric::runtime {
	class Line;
} // namespace lyric::runtime

namespace music_lyric_player::rendering::common {
	struct RenderContext;
} // namespace music_lyric_player::rendering::common

namespace music_lyric_player::rendering::components::line::normal::main::syllable {
	class Word;

	/**
	 * A word-timed normal-line body that caches and manually places one paragraph per word.
	 */
	class Element {
	public:
		/**
		 * Builds timed word components and preserves explicit spaces between them.
		 */
		explicit Element(const ::lyric::runtime::Line& info);

		/**
		 * Destroys cached word components where their concrete type is complete.
		 */
		~Element();

		/**
		 * Shapes words, wraps them into rows, and aligns every row within the available width.
		 */
		void layout(float width, const common::RenderContext& context);

		/**
		 * Paints every cached word with karaoke reveal and float animation.
		 */
		void paint(SkCanvas* canvas, float x, float y, double now, bool active, const common::RenderContext& context) const;

		/**
		 * Returns the total laid-out row height in logical pixels.
		 */
		float height() const;

	private:
		std::vector<std::unique_ptr<Word>> words;
		mutable animation::Mask            mask;
		float                              width          = 0.0f;
		float                              measuredHeight = 0.0f;
	};
} // namespace music_lyric_player::rendering::components::line::normal::main::syllable

#endif // MUSIC_LYRIC_PLAYER_RENDERING_COMPONENTS_LINE_NORMAL_MAIN_SYLLABLE_INDEX_H_
