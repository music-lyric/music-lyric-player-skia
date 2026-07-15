#ifndef MUSIC_LYRIC_PLAYER_RENDERING_COMPONENTS_LINE_NORMAL_MAIN_SYLLABLE_ANIMATION_MASK_H_
#define MUSIC_LYRIC_PLAYER_RENDERING_COMPONENTS_LINE_NORMAL_MAIN_SYLLABLE_ANIMATION_MASK_H_

#include <cstddef>
#include <vector>

#include "include/core/SkColor.h"
#include "include/core/SkRect.h"

class SkCanvas;

namespace music_lyric_player::rendering::components::line::normal::main::syllable::animation {
	/**
	 * Line-wide karaoke mask timeline that converts word timing and geometry into per-word reveal frames.
	 */
	class Mask {
	public:
		/**
		 * Timing and measured geometry of one normal word.
		 */
		struct Input {
			double start    = 0.0;
			double duration = 0.0;
			float  width    = 0.0f;
			float  height   = 0.0f;
		};

		/**
		 * Creates an empty mask timeline relative to the line's absolute start.
		 */
		Mask(double lineStart, double lineDuration);

		/**
		 * Rebuilds timing segments and prefix widths after word layout changes.
		 */
		void update(const std::vector<Input>& inputs);

		/**
		 * Samples all per-word reveal frames at the raw playback time.
		 */
		void sample(double currentTime, double normal, double first, double last) const;

		/**
		 * Returns one word's sampled normalized reveal progress.
		 */
		float progress(std::size_t index) const;

		/**
		 * Returns one word's sampled feather width.
		 */
		float feather(std::size_t index) const;

		/**
		 * Wipes the current glyph layer's alpha from the unsung color to the sung color, left to right.
		 */
		static void apply(SkCanvas* canvas, const SkRect& drawBounds, const SkRect& textBounds, float progress, float feather, SkColor unsungColor, SkColor sungColor);

	private:
		struct Segment {
			Input  input;
			double moveStart    = 0.0;
			double moveDuration = 0.0;
			float  prefix       = 0.0f;
		};

		struct Frame {
			float progress = 0.0f;
			float feather  = 0.0f;
		};

		double                     lineStart;
		double                     lineDuration;
		std::vector<Segment>       segments;
		mutable std::vector<float> phases;
		mutable std::vector<Frame> frames;
	};
} // namespace music_lyric_player::rendering::components::line::normal::main::syllable::animation

#endif // MUSIC_LYRIC_PLAYER_RENDERING_COMPONENTS_LINE_NORMAL_MAIN_SYLLABLE_ANIMATION_MASK_H_
