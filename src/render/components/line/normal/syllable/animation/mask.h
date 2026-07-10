#ifndef MUSIC_LYRIC_PLAYER_RENDER_COMPONENTS_LINE_NORMAL_SYLLABLE_ANIMATION_MASK_H_
#define MUSIC_LYRIC_PLAYER_RENDER_COMPONENTS_LINE_NORMAL_SYLLABLE_ANIMATION_MASK_H_

#include <cstddef>

#include "include/core/SkRect.h"

class SkCanvas;

namespace music_lyric_player::render::components::line::normal::syllable::animation {
	/**
	 * Word-local karaoke reveal timing and soft-edge sizing.
	 */
	class Mask {
	public:
		/**
		 * Caches the word timing and its position within the line.
		 */
		Mask(double start, double duration, std::size_t index, std::size_t count);

		/**
		 * Returns normalized reveal progress at the raw playback time.
		 */
		float progress(double currentTime) const;

		/**
		 * Returns a clamped feather width derived from word height and config values.
		 */
		float feather(float height, double normal, double first, double last) const;

		/**
		 * Multiplies the current layer by the soft left-to-right reveal gradient.
		 */
		void apply(SkCanvas* canvas, const SkRect& bounds, float progress, float feather) const;

	private:
		double      start;
		double      duration;
		std::size_t wordIndex;
		std::size_t wordCount;
	};
} // namespace music_lyric_player::render::components::line::normal::syllable::animation

#endif // MUSIC_LYRIC_PLAYER_RENDER_COMPONENTS_LINE_NORMAL_SYLLABLE_ANIMATION_MASK_H_
