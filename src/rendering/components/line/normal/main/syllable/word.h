#ifndef MUSIC_LYRIC_PLAYER_RENDERING_COMPONENTS_LINE_NORMAL_MAIN_SYLLABLE_WORD_H_
#define MUSIC_LYRIC_PLAYER_RENDERING_COMPONENTS_LINE_NORMAL_MAIN_SYLLABLE_WORD_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "include/core/SkColor.h"
#include "rendering/components/line/normal/main/syllable/animation/index.h"
#include "rendering/utils/fragment/group.h"

class SkCanvas;

#include "music_lyric_model.h"

namespace music_lyric_player::rendering::common {
	struct RenderContext;
} // namespace music_lyric_player::rendering::common

namespace music_lyric_player::rendering::components::line::normal::main::syllable {
	/**
	 * One cached timed-word paragraph with karaoke reveal and vertical float animation.
	 */
	class Word {
	public:
		/**
		 * Copies the word text and timing needed for frame-independent animation sampling.
		 */
		Word(const music_lyric_model::common::WordNormal& info, uint32_t spacesBefore);

		/**
		 * Destroys the cached fragment group where its concrete type is complete.
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
		 * `inactiveOpacity` is the line-wide word opacity used while inactive, eased by the owning element so a deactivating line fades out instead of snapping.
		 */
		void paint(SkCanvas*                 canvas,
			float                        lineX,
			float                        lineY,
			double                       now,
			bool                         active,
			bool                         maskEnabled,
			float                        maskProgress,
			float                        maskFeather,
			float                        inactiveOpacity,
			const common::RenderContext& context) const;

		/**
		 * Returns the timing and measured geometry consumed by the line-wide mask host.
		 */
		animation::Mask::Input maskInput() const;

		/**
		 * Returns the number of explicit lyric spaces preceding this word.
		 */
		uint32_t spacesBefore() const;

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
		 * Resolved glow inputs for one frame: the halo color and the word-level blur radius.
		 */
		struct GlowPaint {
			SkColor color  = SK_ColorTRANSPARENT;
			float   radius = 0.0f;
		};

		/**
		 * Paints the cached word blob — or its per-character cells with their emphasize transforms — at the word position in the supplied state color.
		 */
		void paintGroup(SkCanvas* canvas, float x, float y, SkColor color, const std::vector<animation::Emphasize::Transform>* transforms) const;

		/**
		 * Paints the blurred glow halo beneath every emphasized cell whose pulse is lit, scaling each halo with its cell transform.
		 * `opacity` is the paint path's state multiplier, mirroring how the web mask or word opacity also dims the char shadows.
		 */
		void paintGlow(SkCanvas* canvas, float x, float y, const GlowPaint& glow, double opacity, const std::vector<animation::Emphasize::Transform>* transforms) const;

		/**
		 * Reveals the word by wiping alpha from the unsung color to the sung color across its width.
		 * A lit glow renders in its own layer beneath the body, wiped by the same band with kDstIn so the halo keeps its color while its alpha follows the mask.
		 */
		void paintReveal(SkCanvas*                                  canvas,
			float                                               x,
			float                                               y,
			float                                               progress,
			float                                               feather,
			SkColor                                             unsungColor,
			SkColor                                             sungColor,
			const std::vector<animation::Emphasize::Transform>* transforms,
			const GlowPaint*                                    glow) const;

		std::string text;
		double      start;
		double      duration;
		uint32_t    spaceCount;
		bool        stressed;

		animation::Float     floating;
		animation::Emphasize emphasizing;

		utils::fragment::FragmentGroup              group;
		std::vector<utils::fragment::FragmentGroup> cells;
		bool                                        useCells         = false;
		float                                       measuredWidth    = 0.0f;
		float                                       measuredHeight   = 0.0f;
		float                                       measuredBaseline = 0.0f;
		float                                       x                = 0.0f;
		float                                       y                = 0.0f;
	};
} // namespace music_lyric_player::rendering::components::line::normal::main::syllable

#endif // MUSIC_LYRIC_PLAYER_RENDERING_COMPONENTS_LINE_NORMAL_MAIN_SYLLABLE_WORD_H_
