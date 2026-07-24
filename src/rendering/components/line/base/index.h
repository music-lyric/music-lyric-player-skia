#ifndef MUSIC_LYRIC_PLAYER_RENDERING_COMPONENTS_LINE_BASE_INDEX_H_
#define MUSIC_LYRIC_PLAYER_RENDERING_COMPONENTS_LINE_BASE_INDEX_H_

#include "include/core/SkColor.h"
#include "rendering/config/layout/index.gen.h"
#include "rendering/utils/animation/tween.h"

class SkCanvas;

namespace music_lyric_player::rendering::common {
	struct RenderContext;
} // namespace music_lyric_player::rendering::common

namespace music_lyric_player::rendering::components::line::base {
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
		 * Paints the line with its top-left at (`x`, `y`), sampling any per-frame animation at `now` and styling for the active, played, or normal state.
		 * `played` marks a line already sung (before the active line); it drives the dimmer played styling.
		 */
		virtual void paint(SkCanvas* canvas, float x, float y, double now, bool active, bool played, const common::RenderContext& context) const = 0;

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
		    : lineIndex(index) {
			this->colorTween.setEasing(animation::inOutCubic);
		}

		/**
		 * The x of the left edge at which a rigid block of `blockWidth` sits inside `[x, x + width)` for `align`.
		 * Shared horizontal-alignment policy for line kinds that place a rigid block.
		 */
		float alignBlockX(float x, float blockWidth, config::layout::Align align) const {
			using Align = config::layout::Align;
			switch (align) {
			case Align::Center:
				return x + (this->width - blockWidth) * 0.5f;
			case Align::Right:
				return x + (this->width - blockWidth);
			case Align::Left:
			default:
				return x;
			}
		}

		/**
		 * Advances the shared line tint to `now` and returns it, easing across normal / played / active changes.
		 * The first call seeds the colour without animating; a stable state tracks config colour edits without restarting the ease.
		 * `played` selects the dimmer played tint for a line already sung; callers without a played state leave it unset.
		 */
		SkColor stateColor(double now, bool active, SkColor normalColor, SkColor activeColor, bool played = false, SkColor playedColor = SK_ColorTRANSPARENT) const {
			const SkColor target = active ? activeColor : (played ? playedColor : normalColor);
			const int     state  = active ? 2 : (played ? 1 : 0);
			if (!this->colorReady) {
				this->colorTween.snap(target);
				this->colorState = state;
				this->colorReady = true;
			} else if (state != this->colorState) {
				this->colorTween.setDuration(kStateColorDuration);
				this->colorTween.retarget(now, target);
				this->colorState = state;
			} else {
				this->colorTween.setTarget(target);
			}
			return this->colorTween.sample(now);
		}

		int   lineIndex;
		float measuredHeight = 0.0f;
		float width          = 0.0f; // content width the line was laid out to

	private:
		// Line colour transition; a dedicated config (duration / easing) is not ported yet (M2.3+).
		static constexpr double kStateColorDuration = 300.0;

		mutable animation::Tween<SkColor> colorTween;
		mutable int                       colorState = 0;     // last state the tint was resolved for: 0 normal / 1 played / 2 active
		mutable bool                      colorReady = false; // whether the tint tween has been seeded
	};
} // namespace music_lyric_player::rendering::components::line::base

#endif // MUSIC_LYRIC_PLAYER_RENDERING_COMPONENTS_LINE_BASE_INDEX_H_
