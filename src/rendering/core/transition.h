#ifndef MUSIC_LYRIC_PLAYER_RENDERING_CORE_TRANSITION_H_
#define MUSIC_LYRIC_PLAYER_RENDERING_CORE_TRANSITION_H_

#include <string>

namespace music_lyric_player::rendering::config::scroll {
	struct AnimationConfig;
} // namespace music_lyric_player::rendering::config::scroll

namespace music_lyric_player::rendering::core {
	/**
	 * Duration and easing selected by the active scroll animation mode.
	 */
	struct TransitionTiming {
		double               duration;
		const ::std::string& easing;
	};

	/**
	 * Per-line transition timing (duration and start delay) of the scroll cascade.
	 */
	struct Transition {
		double duration;
		double delay;
	};

	/**
	 * Selects the duration and easing owned by `anim`'s active mode.
	 */
	TransitionTiming transitionTiming(const config::scroll::AnimationConfig& anim);

	/**
	 * Computes one line's transition duration and start delay for the cascade `anim` describes.
	 * `offset` is the line's signed distance from the new active line, and `direction` is the sign of the latest active-line change (`0` when unchanged).
	 * Shared by the scroll and effect managers so both cascade with the same per-line delay.
	 */
	Transition lineTransition(const config::scroll::AnimationConfig& anim, int offset, int direction);
} // namespace music_lyric_player::rendering::core

#endif // MUSIC_LYRIC_PLAYER_RENDERING_CORE_TRANSITION_H_
