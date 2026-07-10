#ifndef MUSIC_LYRIC_PLAYER_RENDER_COMPONENTS_LINE_NORMAL_SYLLABLE_ANIMATION_FLOAT_H_
#define MUSIC_LYRIC_PLAYER_RENDER_COMPONENTS_LINE_NORMAL_SYLLABLE_ANIMATION_FLOAT_H_

#include "render/utils/animation/easing.h"
#include "render/utils/animation/tween.h"

namespace music_lyric_player::render::components::line::normal::syllable::animation {
	/**
	 * Content-timed vertical word motion with an eased return after line deactivation.
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
		float sample(double now, double currentTime, bool active, bool enabled, float from, float to) const;

	private:
		double start;
		double duration;

		::music_lyric_player::render::animation::CubicBezier          easing{0.25f, 0.1f, 0.25f, 1.0f};
		mutable ::music_lyric_player::render::animation::Tween<float> tween;
		mutable bool                                                  ready  = false;
		mutable bool                                                  active = false;
	};
} // namespace music_lyric_player::render::components::line::normal::syllable::animation

#endif // MUSIC_LYRIC_PLAYER_RENDER_COMPONENTS_LINE_NORMAL_SYLLABLE_ANIMATION_FLOAT_H_
