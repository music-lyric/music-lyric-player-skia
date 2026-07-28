#ifndef MUSIC_LYRIC_PLAYER_RENDERING_COMPONENTS_LINE_NORMAL_INDEX_H_
#define MUSIC_LYRIC_PLAYER_RENDERING_COMPONENTS_LINE_NORMAL_INDEX_H_

#include <memory>

#include "music_lyric_model.h"
#include "rendering/components/line/base/index.h"
#include "rendering/utils/animation/tween.h"

class SkCanvas;

namespace music_lyric_player::rendering::common {
	struct RenderContext;
} // namespace music_lyric_player::rendering::common

namespace music_lyric_player::rendering::components::line::normal::annotation {
	class Row;
} // namespace music_lyric_player::rendering::components::line::normal::annotation

namespace music_lyric_player::rendering::components::line::normal::main::plain {
	class Element;
} // namespace music_lyric_player::rendering::components::line::normal::main::plain

namespace music_lyric_player::rendering::components::line::normal::main::syllable {
	class Element;
} // namespace music_lyric_player::rendering::components::line::normal::main::syllable

namespace music_lyric_player::rendering::components::line::normal {
	/**
	 * A normal lyric-line shell that selects plain or word-timed content and applies shared line state.
	 */
	class Element : public base::Element {
	public:
		Element(int index, const music_lyric_model::parsed::Line& info, bool isSyllable);

		/**
		 * Destroys the selected body renderer where its concrete type is complete.
		 */
		~Element() override;

		/**
		 * Selects the configured body renderer and delegates layout to it.
		 */
		void layout(float width, const common::RenderContext& context) override;

		/**
		 * Resolves shared colors, applies the eased line-wide base opacity, and delegates painting to the selected body renderer.
		 */
		void paint(SkCanvas* canvas, float x, float y, double now, bool active, bool played, const common::RenderContext& context) const override;

	private:
		/**
		 * Rebuilds the body renderer only when the resolved rendering mode changes.
		 */
		void selectBody(bool useSyllable);

		const music_lyric_model::parsed::Line&   info;
		bool                                     syllableEnable;
		bool                                     syllableMode = false;
		std::unique_ptr<main::plain::Element>    plainElement;
		std::unique_ptr<main::syllable::Element> syllableElement;

		// The optional romanization and translation sub-lines, stacked above and below the main content; null when hidden or empty.
		std::unique_ptr<annotation::Row> romanRow;
		std::unique_ptr<annotation::Row> translateRow;

		// Vertical extents of the three stacked rows, summed into `measuredHeight`; drive the fixed `roman / main / translate` stack offsets.
		float mainHeight      = 0.0f;
		float romanHeight     = 0.0f;
		float translateHeight = 0.0f;

		// The line-wide base opacity, eased across every state change over 0.6s, mirroring the web `.normal` `transition: opacity 0.6s ease`; it multiplies the whole line the way CSS opacity composites onto the children.
		mutable animation::Tween<float> baseOpacity;
		mutable bool                    baseOpacityReady = false;
		mutable float                   baseOpacityGoal  = 0.0f;
	};
} // namespace music_lyric_player::rendering::components::line::normal

#endif // MUSIC_LYRIC_PLAYER_RENDERING_COMPONENTS_LINE_NORMAL_INDEX_H_
