#ifndef MUSIC_LYRIC_PLAYER_RENDERING_COMPONENTS_LINE_NORMAL_ANNOTATION_ANNOTATION_H_
#define MUSIC_LYRIC_PLAYER_RENDERING_COMPONENTS_LINE_NORMAL_ANNOTATION_ANNOTATION_H_

#include <string>

#include "include/core/SkColor.h"
#include "rendering/config/common/index.gen.h"
#include "rendering/config/layout/index.gen.h"
#include "rendering/utils/animation/tween.h"
#include "rendering/utils/fragment/group.h"

class SkCanvas;
class SkFont;

namespace music_lyric_player::rendering::common {
	struct RenderContext;
} // namespace music_lyric_player::rendering::common

namespace music_lyric_player::rendering::components::line::normal::annotation {
	/**
	 * One annotation sub-line (romanization or translation) stacked above or below the main vocal content.
	 * It self-shapes its extracted text into a wrapped paragraph and eases its own per-state tint, so a row wraps and animates independently of the main line while compositing under the shared line base-opacity layer.
	 */
	class Row {
	public:
		/**
		 * Stores the already-extracted row text; an empty string paints nothing.
		 */
		explicit Row(std::string text);

		/**
		 * Destroys the cached fragment group where its concrete type is complete.
		 */
		~Row();

		/**
		 * Shapes and wraps the row text for the available width with the supplied font, resolving per-line alignment.
		 */
		void layout(float width, const common::RenderContext& context, const SkFont& font, config::layout::Align align);

		/**
		 * Eases the row tint across the normal / played / active states and paints the wrapped paragraph at (`x`, `y`).
		 */
		void paint(SkCanvas* canvas, float x, float y, double now, bool active, bool played, const config::common::StateStyleConfig& style) const;

		/**
		 * Returns the laid-out block height in logical pixels; valid after `layout`.
		 */
		float height() const;

		/**
		 * Reports whether the row has no text or produced no paintable fragment.
		 */
		bool empty() const;

	private:
		std::string                    text;
		utils::fragment::FragmentGroup group;
		float                          measuredHeight = 0.0f;

		// The row tint, eased across state flips exactly like the shared line tint (`base::Element::stateColor`); each row keeps its own so a mid-fade layout never restarts it.
		mutable animation::Tween<SkColor> colorTween;
		mutable int                       colorState = 0;     // last state the tint was resolved for: 0 normal / 1 played / 2 active
		mutable bool                      colorReady = false; // whether the tint tween has been seeded
	};
} // namespace music_lyric_player::rendering::components::line::normal::annotation

#endif // MUSIC_LYRIC_PLAYER_RENDERING_COMPONENTS_LINE_NORMAL_ANNOTATION_ANNOTATION_H_
