#ifndef MUSIC_LYRIC_PLAYER_PLATFORM_ANDROID_CLOCK_H_
#define MUSIC_LYRIC_PLAYER_PLATFORM_ANDROID_CLOCK_H_

#include "utils/clock/clock.h"
#include "utils/clock/steady.h"

namespace music_lyric_player::platform::android {
	/**
	 * A clock the app can take over, for driving playback from its own time source such as a `MediaPlayer` or an ExoPlayer.
	 * It reports steady time until `set()` is first called, so a player never handed an external time still runs off wall time exactly as it does on desktop.
	 * The app is expected to push a fresh time once per frame on the render thread, right before ticking the player.
	 */
	class ManualClock : public utils::Clock {
	public:
		/**
		 * Returns the time the app last set, or steady time while the app has not taken over.
		 */
		double now() const override {
			return this->external ? this->current : this->steady.now();
		}

		/**
		 * Sets the time later `now()` calls report, taking the clock over from steady time for good.
		 * A source that seeks reports a discontinuity here, which the player only reads as a jump in elapsed time.
		 * Seeking the player deliberately is still `play(seek)`.
		 */
		void set(double now) {
			this->external = true;
			this->current  = now;
		}

	private:
		utils::SteadyClock steady;

		bool   external = false;
		double current  = 0.0;
	};
} // namespace music_lyric_player::platform::android

#endif // MUSIC_LYRIC_PLAYER_PLATFORM_ANDROID_CLOCK_H_
