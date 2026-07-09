#ifndef MUSIC_LYRIC_PLAYER_RENDER_CORE_TRANSITION_H_
#define MUSIC_LYRIC_PLAYER_RENDER_CORE_TRANSITION_H_

namespace music_lyric_player::render::config::scroll {
	struct AnimationConfig;
} // namespace music_lyric_player::render::config::scroll

namespace music_lyric_player::render::core {
	/**
	 * Per-line transition timing (duration and start delay) of the scroll cascade.
	 */
	struct Transition {
		double duration;
		double delay;
	};

	/**
	 * Computes one line's transition duration and start delay for the cascade `anim` describes.
	 * `offset` is the line's signed distance from the active line, `played` whether it precedes it, and `direction` the sign of the latest active-line change (`0` when unchanged).
	 * Shared by the scroll and effect managers so both cascade with the same per-line delay.
	 */
	Transition lineTransition(const config::scroll::AnimationConfig& anim, int offset, bool played, int direction);
} // namespace music_lyric_player::render::core

#endif // MUSIC_LYRIC_PLAYER_RENDER_CORE_TRANSITION_H_
