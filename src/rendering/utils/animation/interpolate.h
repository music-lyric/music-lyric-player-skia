#ifndef MUSIC_LYRIC_PLAYER_RENDERING_UTILS_ANIMATION_INTERPOLATE_H_
#define MUSIC_LYRIC_PLAYER_RENDERING_UTILS_ANIMATION_INTERPOLATE_H_

#include "include/core/SkColor.h"

namespace music_lyric_player::rendering::animation {
	/**
	 * Linearly interpolates from `from` to `to` by factor `t` (0 yields `from`, 1 yields `to`).
	 * The default fits any type with `+`, `-` and scalar `*` (float, SkScalar, SkPoint); overload it for types that cannot interpolate arithmetically, such as packed colours.
	 * Uses `from + (to - from) * t` verbatim so a settled endpoint reproduces the source value exactly.
	 */
	template <typename T>
	T lerp(const T& from, const T& to, float t) {
		return from + (to - from) * t;
	}

	/**
	 * Interpolates a packed ARGB colour channel by channel, since the arithmetic default would corrupt the packing.
	 * Each 8-bit channel eases independently in sRGB (gamma) space, matching a CSS colour transition; the result is rounded and clamped.
	 */
	inline SkColor lerp(const SkColor& from, const SkColor& to, float t) {
		const auto channel = [t](U8CPU a, U8CPU b) -> U8CPU {
			const float value   = static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * t;
			const float clamped = value < 0.0f ? 0.0f : (value > 255.0f ? 255.0f : value);
			return static_cast<U8CPU>(clamped + 0.5f);
		};
		return SkColorSetARGB(
		    channel(SkColorGetA(from), SkColorGetA(to)),
		    channel(SkColorGetR(from), SkColorGetR(to)),
		    channel(SkColorGetG(from), SkColorGetG(to)),
		    channel(SkColorGetB(from), SkColorGetB(to)));
	}
} // namespace music_lyric_player::rendering::animation

#endif // MUSIC_LYRIC_PLAYER_RENDERING_UTILS_ANIMATION_INTERPOLATE_H_
