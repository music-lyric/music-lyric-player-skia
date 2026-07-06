#include "render/core/scroll.h"

#include "render/utils/animation/easing.h"

namespace music_lyric_player::render::core {
	ScrollManager::ScrollManager() {
		tween_.setEasing(animation::inOutCubic);
	}

	void ScrollManager::reset() {
		initialized_ = false;
		focus_       = -1;
	}

	float ScrollManager::update(double nowMs, float target, int focus, double durationMs) {
		tween_.setDuration(durationMs);
		if (!initialized_) {
			tween_.snap(target);
			focus_       = focus;
			initialized_ = true;
		} else if (focus != focus_) {
			tween_.retarget(nowMs, target);
			focus_ = focus;
		} else if (tween_.finished(nowMs)) {
			// Settled under a stable focus: snap so the offset tracks any layout drift immediately.
			tween_.snap(target);
		} else {
			tween_.setTarget(target);
		}
		return tween_.sample(nowMs);
	}
} // namespace music_lyric_player::render::core
