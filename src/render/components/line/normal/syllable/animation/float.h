#ifndef MUSIC_LYRIC_PLAYER_RENDER_COMPONENTS_LINE_NORMAL_SYLLABLE_ANIMATION_FLOAT_H_
#define MUSIC_LYRIC_PLAYER_RENDER_COMPONENTS_LINE_NORMAL_SYLLABLE_ANIMATION_FLOAT_H_

#include "render/utils/animation/easing.h"

namespace music_lyric_player::render::components::line::normal::syllable::animation {
	/**
	 * Content-timed vertical word motion sampled directly from playback time.
	 */
	class Float {
	public:
		/**
		 * Caches the word's absolute start and non-negative duration.
		 */
		Float(double start, double duration);

		/**
		 * Samples the configured offset for the current playback and active state.
		 */
		float sample(double currentTime, bool active, bool enabled, float from, float to) const;

	private:
		double start;
		double duration;

		::music_lyric_player::render::animation::CubicBezier easing{0.25f, 0.1f, 0.25f, 1.0f};
	};
} // namespace music_lyric_player::render::components::line::normal::syllable::animation

#endif // MUSIC_LYRIC_PLAYER_RENDER_COMPONENTS_LINE_NORMAL_SYLLABLE_ANIMATION_FLOAT_H_
