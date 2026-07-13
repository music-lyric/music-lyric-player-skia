#ifndef MUSIC_LYRIC_PLAYER_RENDER_COMPONENTS_LINE_NORMAL_INDEX_H_
#define MUSIC_LYRIC_PLAYER_RENDER_COMPONENTS_LINE_NORMAL_INDEX_H_

#include <memory>

#include "render/components/line/base/index.h"

class SkCanvas;

namespace lyric::runtime {
	class Line;
} // namespace lyric::runtime

namespace music_lyric_player::render::common {
	struct RenderContext;
} // namespace music_lyric_player::render::common

namespace music_lyric_player::render::components::line::normal::main::plain {
	class Element;
} // namespace music_lyric_player::render::components::line::normal::main::plain

namespace music_lyric_player::render::components::line::normal::main::syllable {
	class Element;
} // namespace music_lyric_player::render::components::line::normal::main::syllable

namespace music_lyric_player::render::components::line::normal {
	/**
	 * A normal lyric-line shell that selects plain or word-timed content and applies shared line state.
	 */
	class Element : public base::Element {
	public:
		/**
		 * Creates a normal line backed by the source runtime line and timing mode.
		 */
		Element(int index, const ::lyric::runtime::Line& info, bool isSyllable);

		/**
		 * Destroys the selected body renderer where its concrete type is complete.
		 */
		~Element() override;

		/**
		 * Selects the configured body renderer and delegates layout to it.
		 */
		void layout(float width, const common::RenderContext& context) override;

		/**
		 * Resolves shared colors and delegates painting to the selected body renderer.
		 */
		void paint(SkCanvas* canvas, float x, float y, double now, bool active, const common::RenderContext& context) const override;

	private:
		/**
		 * Rebuilds the body renderer only when the resolved rendering mode changes.
		 */
		void selectBody(bool useSyllable);

		const ::lyric::runtime::Line&      info;
		bool                               syllableEnable;
		bool                               syllableMode = false;
		std::unique_ptr<main::plain::Element>    plainElement;
		std::unique_ptr<main::syllable::Element> syllableElement;
	};
} // namespace music_lyric_player::render::components::line::normal

#endif // MUSIC_LYRIC_PLAYER_RENDER_COMPONENTS_LINE_NORMAL_INDEX_H_
