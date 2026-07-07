#ifndef MUSIC_LYRIC_PLAYER_RENDER_COMPONENTS_LINE_NORMAL_INDEX_H_
#define MUSIC_LYRIC_PLAYER_RENDER_COMPONENTS_LINE_NORMAL_INDEX_H_

#include <memory>
#include <string>

#include "render/components/line/base/index.h"

class SkCanvas;

namespace skia::textlayout {
	class Paragraph;
} // namespace skia::textlayout

namespace music_lyric_player::render::common {
	struct RenderContext;
} // namespace music_lyric_player::render::common

namespace music_lyric_player::render::components::line::normal {
	/**
	 * A whole-line plain-text component: builds one SkParagraph and paints it tinted by line state.
	 */
	class Element : public base::Element {
	public:
		/**
		 * Creates the line for lyric index `index` carrying plain `text`.
		 */
		Element(int index, std::string text);

		/**
		 * Defined in the .cpp so the incomplete `Paragraph` is complete at the deletion point.
		 */
		~Element() override;

		void layout(float width, const common::RenderContext& context) override;
		void paint(SkCanvas* canvas, float x, float y, double now, bool active, const common::RenderContext& context) const override;

	private:
		std::string                                    text;
		std::unique_ptr<::skia::textlayout::Paragraph> paragraph;
	};
} // namespace music_lyric_player::render::components::line::normal

#endif // MUSIC_LYRIC_PLAYER_RENDER_COMPONENTS_LINE_NORMAL_INDEX_H_
