#ifndef MUSIC_LYRIC_PLAYER_RENDER_COMPONENTS_LINE_BASE_INDEX_H_
#define MUSIC_LYRIC_PLAYER_RENDER_COMPONENTS_LINE_BASE_INDEX_H_

class SkCanvas;

namespace music_lyric_player::render::common {
	struct RenderContext;
} // namespace music_lyric_player::render::common

namespace music_lyric_player::render::components::line::base {
	/**
	 * Abstract lyric line: lays itself out to a content width and paints itself, styled by active state.
	 * Concrete subclasses (normal / interlude) provide the geometry and paint.
	 */
	class Element {
	public:
		virtual ~Element() = default;

		/**
		 * (Re)lays the line out for the given content width, refreshing the measured height.
		 */
		virtual void layout(float width, const common::RenderContext& context) = 0;

		/**
		 * Paints the line with its top-left at (`x`, `y`), styled for the active or normal state.
		 */
		virtual void paint(SkCanvas* canvas, float x, float y, bool active, const common::RenderContext& context) const = 0;

		/**
		 * The measured height in logical pixels; valid after `layout`.
		 */
		float height() const {
			return this->measuredHeight;
		}

		/**
		 * The lyric line index this component maps to.
		 */
		int index() const {
			return this->lineIndex;
		}

	protected:
		explicit Element(int index)
		    : lineIndex(index) {}

		/**
		 * The x of the left edge at which a rigid block of `blockWidth` sits inside `[x, x + width)` for `align`.
		 * Shared horizontal-alignment policy (0 left / 1 center / 2 right) for line kinds that place a block.
		 */
		float alignBlockX(float x, float blockWidth, int align) const {
			switch (align) {
			case 1:
				return x + (this->width - blockWidth) * 0.5f; // center
			case 2:
				return x + (this->width - blockWidth); // right
			default:
				return x; // left
			}
		}

		int   lineIndex;
		float measuredHeight = 0.0f;
		float width          = 0.0f; // content width the line was laid out to
	};
} // namespace music_lyric_player::render::components::line::base

#endif // MUSIC_LYRIC_PLAYER_RENDER_COMPONENTS_LINE_BASE_INDEX_H_
