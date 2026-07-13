#ifndef MUSIC_LYRIC_PLAYER_RENDER_COMPONENTS_LINE_NORMAL_MAIN_PLAIN_INDEX_H_
#define MUSIC_LYRIC_PLAYER_RENDER_COMPONENTS_LINE_NORMAL_MAIN_PLAIN_INDEX_H_

#include <string>
#include <vector>

#include "include/core/SkColor.h"
#include "include/core/SkRefCnt.h"

class SkCanvas;
class SkTextBlob;

namespace lyric::runtime {
	class Line;
} // namespace lyric::runtime

namespace music_lyric_player::render::common {
	struct RenderContext;
} // namespace music_lyric_player::render::common

namespace music_lyric_player::render::components::line::normal::main::plain {
	/**
	 * A plain normal-line body that self-shapes the complete line into per-line cached text blobs.
	 * It wraps the text to the available width and bakes the alignment offset into each line at layout time.
	 */
	class Element {
	public:
		/**
		 * Extracts and stores the line's complete plain text.
		 */
		explicit Element(const ::lyric::runtime::Line& info);

		/**
		 * Destroys the cached blobs where the blob type is complete.
		 */
		~Element();

		/**
		 * Shapes and wraps the complete line for the available width and resolves per-line alignment.
		 */
		void layout(float width, const common::RenderContext& context);

		/**
		 * Paints every wrapped line with the supplied resolved line color.
		 */
		void paint(SkCanvas* canvas, float x, float y, SkColor color) const;

		/**
		 * Returns the laid-out block height in logical pixels.
		 */
		float height() const;

	private:
		/**
		 * One shaped and wrapped line whose glyph positions already sit at their block-absolute baseline.
		 * The alignment offset is applied along x at paint time.
		 */
		struct PaintLine {
			sk_sp<SkTextBlob> blob;
			float             offsetX = 0.0f;
		};

		std::string            text;
		std::vector<PaintLine> lines;
		float                  width          = 0.0f;
		float                  measuredHeight = 0.0f;
	};
} // namespace music_lyric_player::render::components::line::normal::main::plain

#endif // MUSIC_LYRIC_PLAYER_RENDER_COMPONENTS_LINE_NORMAL_MAIN_PLAIN_INDEX_H_
