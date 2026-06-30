#ifndef MUSIC_LYRIC_PLAYER_UTILS_CLOCK_CLOCK_H_
#define MUSIC_LYRIC_PLAYER_UTILS_CLOCK_CLOCK_H_

namespace music_lyric_player {
	/**
	 * Monotonic time source injected by the platform.
	 * The core pulls `now` on demand and never owns a clock loop.
	 */
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
