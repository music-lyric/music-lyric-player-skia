#ifndef MUSIC_LYRIC_PLAYER_RENDER_COMPONENTS_LINE_INTERLUDE_INDEX_H_
#define MUSIC_LYRIC_PLAYER_RENDER_COMPONENTS_LINE_INTERLUDE_INDEX_H_

#include "render/components/line/base/index.h"

class SkCanvas;

namespace music_lyric_player::render::common {
	struct RenderContext;
} // namespace music_lyric_player::render::common

namespace music_lyric_player::render::components::line::interlude {
	/**
	 * An interlude (instrumental gap) component: a fixed row of dots standing in for a silent line.
	 * Per-dot animation is deferred to M2.3; this paints the dots statically, tinted by line state.
	 */
	class Element : public base::Element {
	public:
		explicit Element(int index);

		void layout(float width, const common::RenderContext& context) override;
		void paint(SkCanvas* canvas, float x, float y, double now, bool active, const common::RenderContext& context) const override;

	private:
		static constexpr int kDotCount = 3;
	};
} // namespace music_lyric_player::render::components::line::interlude

#endif // MUSIC_LYRIC_PLAYER_RENDER_COMPONENTS_LINE_INTERLUDE_INDEX_H_
