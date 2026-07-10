#ifndef MUSIC_LYRIC_PLAYER_RENDER_COMPONENTS_LINE_NORMAL_SYLLABLE_INDEX_H_
#define MUSIC_LYRIC_PLAYER_RENDER_COMPONENTS_LINE_NORMAL_SYLLABLE_INDEX_H_

#include <memory>
#include <vector>

class SkCanvas;

namespace lyric::runtime {
	class Line;
} // namespace lyric::runtime

namespace music_lyric_player::render::common {
	struct RenderContext;
} // namespace music_lyric_player::render::common

namespace music_lyric_player::render::components::line::normal::syllable {
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
		float                              width          = 0.0f;
		float                              measuredHeight = 0.0f;
	};
} // namespace music_lyric_player::render::components::line::normal::syllable

#endif // MUSIC_LYRIC_PLAYER_RENDER_COMPONENTS_LINE_NORMAL_SYLLABLE_INDEX_H_
