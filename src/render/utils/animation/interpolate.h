#ifndef MUSIC_LYRIC_PLAYER_RENDER_UTILS_ANIMATION_INTERPOLATE_H_
#define MUSIC_LYRIC_PLAYER_RENDER_UTILS_ANIMATION_INTERPOLATE_H_

namespace music_lyric_player::render::animation {
	/**
	 * Linearly interpolates from `from` to `to` by factor `t` (0 yields `from`, 1 yields `to`).
	 * The default fits any type with `+`, `-` and scalar `*` (float, SkScalar, SkPoint); overload it for types that cannot interpolate arithmetically, such as packed colours.
	 * Uses `from + (to - from) * t` verbatim so a settled endpoint reproduces the source value exactly.
	 */
	template <typename T>
	T lerp(const T& from, const T& to, float t) {
		return from + (to - from) * t;
	}
} // namespace music_lyric_player::render::animation

#endif // MUSIC_LYRIC_PLAYER_RENDER_UTILS_ANIMATION_INTERPOLATE_H_
