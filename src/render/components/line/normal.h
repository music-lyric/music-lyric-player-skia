#ifndef MUSIC_LYRIC_PLAYER_RENDER_COMPONENTS_LINE_NORMAL_H_
#define MUSIC_LYRIC_PLAYER_RENDER_COMPONENTS_LINE_NORMAL_H_

#include <memory>
#include <string>

class SkCanvas;

namespace skia::textlayout {
	class Paragraph;
} // namespace skia::textlayout

namespace music_lyric_player::render::common {
	struct RenderContext;
} // namespace music_lyric_player::render::common

namespace music_lyric_player::render::components::line {
	/**
	 * A whole-line plain-text component: builds one SkParagraph and paints it tinted by line state.
	 * Interlude lines are held as zero-height placeholders until a dedicated component lands in M2.3.
	 */
	class Normal {
	public:
		/**
		 * Creates the line for lyric index `index` with plain `text`; an interlude line passes empty text and `interlude` true.
		 */
		Normal(int index, std::string text, bool interlude);

		/**
		 * Destroys the line; defined in the .cpp so the incomplete `Paragraph` is complete at the deletion point.
		 */
		~Normal();

		/**
		 * (Re)builds the paragraph wrapped to `width`, refreshing the measured height.
		 */
		void layout(float width, const common::RenderContext& context);

		/**
		 * Paints the line with its top-left at (`x`, `y`), tinting the white paragraph to the active or normal colour.
		 */
		void paint(SkCanvas* canvas, float x, float y, bool active, const common::RenderContext& context) const;

		/**
		 * The measured height in logical pixels; valid after `layout`, zero for an interlude placeholder.
		 */
		float height() const {
			return height_;
		}

		/**
		 * The lyric line index this component maps to.
		 */
		int index() const {
			return index_;
		}

	private:
		int                                            index_;
		std::string                                    text_;
		bool                                           interlude_;
		std::unique_ptr<::skia::textlayout::Paragraph> paragraph_;
		float                                          width_  = 0.0f;
		float                                          height_ = 0.0f;
	};
} // namespace music_lyric_player::render::components::line

#endif // MUSIC_LYRIC_PLAYER_RENDER_COMPONENTS_LINE_NORMAL_H_
