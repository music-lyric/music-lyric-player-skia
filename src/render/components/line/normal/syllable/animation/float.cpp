#include "render/components/line/normal/syllable/animation/float.h"

#include <algorithm>
#include <cmath>

#include "render/utils/animation/interpolate.h"

namespace music_lyric_player::render::components::line::normal::syllable::animation {
	Float::Float(double start, double duration)
	    : start(std::isfinite(start) ? start : 0.0),
	      duration(std::isfinite(duration) ? std::max(duration, 0.0) : 0.0) {}

	float Float::sample(double currentTime, bool active, bool enabled, float from, float to) const {
		if (!enabled || !active) {
			return 0.0f;
		}

		const double animationDuration = std::max(this->duration, 1000.0);
		const double elapsed           = std::isfinite(currentTime) ? currentTime - this->start : 0.0;
		const float  progress          = static_cast<float>(std::clamp(elapsed / animationDuration, 0.0, 1.0));
		return ::music_lyric_player::render::animation::lerp(from, to, this->easing(progress));
	}
} // namespace music_lyric_player::render::components::line::normal::syllable::animation
