#include "rendering/components/line/normal/main/syllable/animation/float.h"

#include <algorithm>
#include <cmath>

#include "rendering/utils/animation/interpolate.h"

namespace music_lyric_player::rendering::components::line::normal::main::syllable::animation {
	Float::Float(double start, double duration)
	    : start(std::isfinite(start) ? start : 0.0),
	      duration(std::isfinite(duration) ? std::max(duration, 0.0) : 0.0) {}

	float Float::sample(double currentTime, double now, bool active, bool enabled, float from, float to) const {
		if (!enabled) {
			this->lastActive = false;
			this->exiting    = false;
			return 0.0f;
		}

		const double animationDuration = std::max(this->duration, 1000.0);

		// Active line: the float rises on playback content time so it stays in sync with the sung word across pause and seek.
		if (active) {
			const double elapsed  = std::isfinite(currentTime) ? currentTime - this->start : 0.0;
			const float  progress = static_cast<float>(std::clamp(elapsed / animationDuration, 0.0, 1.0));
			this->lastActive      = true;
			this->exiting         = false;
			this->phase           = progress;
			return ::music_lyric_player::rendering::animation::lerp(from, to, this->easing(progress));
		}

		// Line just went inactive: replay the same curve backward on wall-clock time so every word eases back instead of snapping to rest.
		if (this->lastActive) {
			this->lastActive = false;
			this->exiting    = this->phase > 0.0f;
			this->exitStart  = now;
		}

		// Settle on wall-clock time so the reverse keeps easing even while playback is paused, mirroring the web reverse.
		if (this->exiting) {
			const double back     = std::isfinite(now) ? (now - this->exitStart) / animationDuration : static_cast<double>(this->phase);
			const float  progress = static_cast<float>(std::clamp(static_cast<double>(this->phase) - back, 0.0, 1.0));
			if (progress <= 0.0f) {
				this->exiting = false;
				return from;
			}
			return ::music_lyric_player::rendering::animation::lerp(from, to, this->easing(progress));
		}

		return from;
	}
} // namespace music_lyric_player::rendering::components::line::normal::main::syllable::animation
