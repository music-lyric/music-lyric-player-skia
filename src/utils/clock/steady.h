#ifndef MUSIC_LYRIC_PLAYER_UTILS_CLOCK_STEADY_H_
#define MUSIC_LYRIC_PLAYER_UTILS_CLOCK_STEADY_H_

#include "utils/clock/clock.h"

namespace music_lyric_player {
	/**
	 * Default `Clock` backed by `std::chrono::steady_clock` (portable std, no platform API).
	 */
	class SteadyClock : public Clock {
	public:
		/**
		 * Returns the steady-clock time since epoch in milliseconds.
		 */
		double nowMs() const override;
	};
} // namespace music_lyric_player

#endif
