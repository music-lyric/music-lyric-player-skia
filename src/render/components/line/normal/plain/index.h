#ifndef MUSIC_LYRIC_PLAYER_RENDER_COMPONENTS_LINE_NORMAL_PLAIN_INDEX_H_
#define MUSIC_LYRIC_PLAYER_RENDER_COMPONENTS_LINE_NORMAL_PLAIN_INDEX_H_

#include <memory>
#include <string>

#include "include/core/SkColor.h"

class SkCanvas;

namespace lyric::runtime {
	class Line;
} // namespace lyric::runtime

namespace skia::textlayout {
	class Paragraph;
} // namespace skia::textlayout

namespace music_lyric_player::render::common {
	struct RenderContext;
} // namespace music_lyric_player::render::common

namespace music_lyric_player::render::components::line::normal::plain {
	/**
	 * A plain normal-line body that shapes the complete line into one cached paragraph.
	 */
	class Element {
	public:
		/**
		 * Extracts and stores the line's complete plain text.
		 */
		explicit Element(const ::lyric::runtime::Line& info);

		/**
		 * Destroys the cached paragraph where its concrete type is complete.
		 */
		~Element();

		/**
		 * Shapes and lays out the complete line for the available width.
		 */
		void layout(float width, const common::RenderContext& context);

		/**
		 * Paints the cached paragraph with the supplied resolved line color.
		 */
		void paint(SkCanvas* canvas, float x, float y, SkColor color) const;

		/**
		 * Returns the laid-out paragraph height in logical pixels.
		 */
		float height() const;

	private:
		std::string                                    text;
		std::unique_ptr<::skia::textlayout::Paragraph> paragraph;
		float                                          width          = 0.0f;
		float                                          measuredHeight = 0.0f;
	};
} // namespace music_lyric_player::render::components::line::normal::plain

#endif // MUSIC_LYRIC_PLAYER_RENDER_COMPONENTS_LINE_NORMAL_PLAIN_INDEX_H_
