#include "render/core/scroll.h"

#include <cstddef>

#include "render/config/scroll/index.gen.h"
#include "render/utils/animation/easing.h"

namespace music_lyric_player::render::core {
	void ScrollManager::reset() {
		this->offsets.clear();
		this->initialized = false;
		this->focus       = -1;
	}

	void ScrollManager::update(double now, float target, int focus, ::std::size_t lineCount, const config::scroll::AnimationConfig& anim) {
		if (this->offsets.size() != lineCount) {
			this->offsets.assign(lineCount, animation::Tween<float>{});
			this->initialized = false;
		}
		if (lineCount == 0) {
			return;
		}

		// First frame of a lyric: snap every line to the target so it does not slide in from nowhere.
		if (!this->initialized) {
			const animation::Easing easing = animation::resolveEasing(anim.easing);
			for (auto& tween : this->offsets) {
				tween.setEasing(easing);
				tween.snap(target);
			}
			this->focus       = focus;
			this->lastTarget  = target;
			this->initialized = true;
			return;
		}

		// Active line changed: restart each line's ease with the delay its mode assigns.
		if (focus != this->focus) {
			const animation::Easing easing    = animation::resolveEasing(anim.easing);
			const int               direction = focus > this->focus ? 1 : (focus < this->focus ? -1 : 0);
			for (::std::size_t i = 0; i < this->offsets.size(); ++i) {
				const Transition transition = lineTransition(anim, static_cast<int>(i) - focus, static_cast<int>(i) < focus, direction);
				auto&            tween      = this->offsets[i];
				tween.setEasing(easing);
				tween.setDuration(transition.duration);
				tween.retarget(now, target, transition.delay);
			}
			this->focus      = focus;
			this->lastTarget = target;
			return;
		}

		// Stable focus: follow any layout drift without restarting the eases.
		if (target != this->lastTarget) {
			for (auto& tween : this->offsets) {
				tween.setTarget(target);
			}
			this->lastTarget = target;
		}
	}
} // namespace music_lyric_player::render::core
