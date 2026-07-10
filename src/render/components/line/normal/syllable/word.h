#ifndef MUSIC_LYRIC_PLAYER_RENDER_COMPONENTS_LINE_NORMAL_SYLLABLE_WORD_H_
#define MUSIC_LYRIC_PLAYER_RENDER_COMPONENTS_LINE_NORMAL_SYLLABLE_WORD_H_

#include <cstddef>
#include <memory>
#include <string>

#include "include/core/SkColor.h"
#include "render/components/line/normal/syllable/animation/index.h"

class SkCanvas;

namespace lyric::runtime {
	class WordNormal;
} // namespace lyric::runtime

namespace skia::textlayout {
	class Paragraph;
} // namespace skia::textlayout

namespace music_lyric_player::render::common {
	struct RenderContext;
} // namespace music_lyric_player::render::common

namespace music_lyric_player::render::components::line::normal::syllable {
	/**
	 * One cached timed-word paragraph with karaoke reveal and vertical float animation.
	 */
	class Word {
	public:
		/**
		 * Copies the word text and timing needed for frame-independent animation sampling.
		 */
		Word(const ::lyric::runtime::WordNormal& info, bool hasSpaceBefore);

		/**
		 * Destroys the cached paragraph where its concrete type is complete.
		 */
		~Word();

		/**
		 * Shapes the word as one unbreakable line and resolves its intrinsic metrics.
		 */
		void layout(const common::RenderContext& context);

		/**
		 * Sets the word's line-relative layout position.
		 */
		void setPosition(float x, float y);

		/**
		 * Paints the inactive word, its timed active-color reveal, and its float transform.
		 */
		void paint(SkCanvas* canvas, float lineX, float lineY, double now, bool active, bool maskEnabled, float maskProgress, float maskFeather, const common::RenderContext& context) const;

		/**
		 * Returns the timing and measured geometry consumed by the line-wide mask host.
		 */
		animation::Mask::Input maskInput() const;

		/**
		 * Returns whether an explicit lyric space precedes this word.
		 */
		bool hasSpaceBefore() const;

		/**
		 * Returns the laid-out word width.
		 */
		float width() const;

		/**
		 * Returns the laid-out word height.
		 */
		float height() const;

		/**
		 * Returns the paragraph alphabetic baseline.
		 */
		float baseline() const;

	private:
		/**
		 * Builds one cached paragraph in the supplied state color.
		 */
		std::unique_ptr<::skia::textlayout::Paragraph> buildParagraph(float width, const common::RenderContext& context, SkColor color) const;

		/**
		 * Paints one cached paragraph at the word position.
		 */
		void paintParagraph(SkCanvas* canvas, ::skia::textlayout::Paragraph& paragraph, float x, float y) const;

		/**
		 * Paints the active-state paragraph through a feathered progress mask.
		 */
		void paintReveal(SkCanvas* canvas, float x, float y, float progress, float feather) const;

		std::string text;
		double      start;
		double      duration;
		bool        spaceBefore;

		animation::Float floating;

		std::unique_ptr<::skia::textlayout::Paragraph> normalParagraph;
		std::unique_ptr<::skia::textlayout::Paragraph> activeParagraph;
		float                                          measuredWidth    = 0.0f;
		float                                          measuredHeight   = 0.0f;
		float                                          measuredBaseline = 0.0f;
		float                                          x                = 0.0f;
		float                                          y                = 0.0f;
	};
} // namespace music_lyric_player::render::components::line::normal::syllable

#endif // MUSIC_LYRIC_PLAYER_RENDER_COMPONENTS_LINE_NORMAL_SYLLABLE_WORD_H_
