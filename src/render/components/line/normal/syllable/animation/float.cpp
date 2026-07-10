#include "render/components/line/normal/syllable/animation/float.h"

#include <algorithm>

#include "render/utils/animation/interpolate.h"

namespace music_lyric_player::render::components::line::normal::syllable::animation {
	Float::Float(double start, double duration)
	    : start(start),
	      duration(std::max(duration, 0.0)) {
		this->tween.setEasing(this->easing);
	}

	float Float::sample(double now, double currentTime, bool active, bool enabled, float from, float to) const {
		if (!enabled) {
			this->tween.snap(0.0f);
			this->ready  = true;
			this->active = active;
			return 0.0f;
		}

		const double animationDuration = std::max(this->duration, 1000.0);
		if (active) {
			const float progress = static_cast<float>(std::clamp((currentTime - this->start) / animationDuration, 0.0, 1.0));
			const float value    = ::music_lyric_player::render::animation::lerp(from, to, this->easing(progress));
			this->tween.snap(value);
			this->ready  = true;
			this->active = true;
			return value;
		}

		if (!this->ready) {
			this->tween.snap(from);
			this->ready = true;
		} else if (this->active) {
			this->tween.setDuration(animationDuration);
			this->tween.retarget(now, from);
		} else {
			this->tween.setTarget(from);
		}
		this->active = false;
		return this->tween.sample(now);
	}
} // namespace music_lyric_player::render::components::line::normal::syllable::animation
