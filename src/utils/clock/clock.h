#ifndef MUSIC_LYRIC_PLAYER_UTILS_CLOCK_CLOCK_H_
#define MUSIC_LYRIC_PLAYER_UTILS_CLOCK_CLOCK_H_

namespace music_lyric_player {
	class Clock {
	public:
		virtual ~Clock() = default;

		/**
		 * Returns a monotonically non-decreasing timestamp in milliseconds.
		 */
		virtual double nowMs() const = 0;
	};
} // namespace music_lyric_player

#endif
