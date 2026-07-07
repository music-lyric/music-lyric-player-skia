#include "render/core/scroll.h"

#include "render/utils/animation/easing.h"

namespace music_lyric_player::render::core {
	ScrollManager::ScrollManager() {
		this->tween.setEasing(animation::inOutCubic);
	}

	void ScrollManager::reset() {
		this->initialized = false;
		this->focus       = -1;
	}

	float ScrollManager::update(double now, float target, int focus, double durationMs) {
		this->tween.setDuration(durationMs);
		if (!this->initialized) {
			this->tween.snap(target);
			this->focus       = focus;
			this->initialized = true;
		} else if (focus != this->focus) {
			this->tween.retarget(now, target);
			this->focus = focus;
		} else if (this->tween.finished(now)) {
			// Settled under a stable focus: snap so the offset tracks any layout drift immediately.
			this->tween.snap(target);
		} else {
			this->tween.setTarget(target);
		}
		return this->tween.sample(now);
	}
} // namespace music_lyric_player::render::core
