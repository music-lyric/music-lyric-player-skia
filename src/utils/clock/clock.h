#ifndef MUSIC_LYRIC_PLAYER_UTILS_CLOCK_CLOCK_H_
#define MUSIC_LYRIC_PLAYER_UTILS_CLOCK_CLOCK_H_

namespace music_lyric_player::utils {
	class Clock {
	public:
		virtual ~Clock() = default;

		/**
		 * Returns a monotonically non-decreasing timestamp in milliseconds.
		 */
		virtual double now() const = 0;
	};
} // namespace music_lyric_player::utils

#endif
