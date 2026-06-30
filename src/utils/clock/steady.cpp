#include "utils/clock/steady.h"

#include <chrono>

namespace music_lyric_player {
	double SteadyClock::nowMs() const {
		const auto since = std::chrono::steady_clock::now().time_since_epoch();
		return std::chrono::duration<double, std::milli>(since).count();
	}
} // namespace music_lyric_player
