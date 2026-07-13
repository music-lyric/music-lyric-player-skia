#ifndef MUSIC_LYRIC_PLAYER_RENDER_COMPONENTS_LINE_NORMAL_MAIN_SYLLABLE_ANIMATION_FLOAT_H_
#define MUSIC_LYRIC_PLAYER_RENDER_COMPONENTS_LINE_NORMAL_MAIN_SYLLABLE_ANIMATION_FLOAT_H_

#include "render/utils/animation/easing.h"

namespace music_lyric_player::render::components::line::normal::main::syllable::animation {
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
		 * Samples the vertical offset for the current playback and active state.
		 * The rise follows playback content time; a line going inactive replays the same curve backward on wall-clock `now` so the whole line settles smoothly instead of snapping to rest.
		 */
		float sample(double currentTime, double now, bool active, bool enabled, float from, float to) const;

	private:
		double start;
		double duration;

		::music_lyric_player::render::animation::CubicBezier easing{0.25f, 0.1f, 0.25f, 1.0f};

		mutable bool   lastActive = false;
		mutable bool   exiting    = false;
		mutable float  phase      = 0.0f;
		mutable double exitStart  = 0.0;
	};
} // namespace music_lyric_player::render::components::line::normal::main::syllable::animation

#endif // MUSIC_LYRIC_PLAYER_RENDER_COMPONENTS_LINE_NORMAL_MAIN_SYLLABLE_ANIMATION_FLOAT_H_
