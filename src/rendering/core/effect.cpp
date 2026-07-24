#include "rendering/core/effect.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "rendering/config/effect/index.gen.h"
#include "rendering/config/scroll/index.gen.h"
#include "rendering/core/transition.h"
#include "rendering/utils/animation/easing.h"

namespace music_lyric_player::rendering::core {
	namespace {
		// Matches web layout.ts: a gaussian of the line-index distance drives the effect intensity.
		constexpr double kSigma  = 2.2;
		constexpr double kCutoff = 4.0 * kSigma;

		/**
		 * Rounds to two decimals, matching the web effect's quantisation.
		 */
		double round2(double value) {
			return ::std::round(value * 100.0) / 100.0;
		}
	} // namespace

	double EffectManager::gaussian(int offset) {
		if (offset > kCutoff || offset < -kCutoff) {
			return 0.0;
		}
		const double o = static_cast<double>(offset);
		return ::std::exp(-(o * o) / (2.0 * kSigma * kSigma));
	}

	float EffectManager::targetScale(const config::effect::Root& effect, int offset) {
		// The active line and a disabled effect both stay at natural size.
		if (offset == 0 || !effect.scale.enabled) {
			return 1.0f;
		}
		const double min = ::std::max(effect.scale.min.value(), 0.0);
		const double max = ::std::max(effect.scale.max.value(), min);
		return static_cast<float>(round2(min + (max - min) * gaussian(offset)));
	}

	float EffectManager::targetBlur(const config::effect::Root& effect, int offset) {
		// The active line and a disabled effect are both perfectly sharp.
		if (offset == 0 || !effect.blur.enabled) {
			return 0.0f;
		}
		const double min = ::std::max(effect.blur.min.value(), 0.0);
		const double max = ::std::max(effect.blur.max.value(), min);
		return static_cast<float>(round2(min + (max - min) * (1.0 - gaussian(offset))));
	}

	void EffectManager::reset() {
		this->scales.clear();
		this->blurs.clear();
		this->initialized = false;
		this->focus       = -1;
	}

	void EffectManager::update(double now, int focus, ::std::size_t lineCount, const config::effect::Root& effect, const config::scroll::AnimationConfig& anim) {
		if (this->scales.size() != lineCount) {
			this->scales.assign(lineCount, animation::Tween<float>{1.0f});
			this->blurs.assign(lineCount, animation::Tween<float>{0.0f});
			this->initialized = false;
		}
		if (lineCount == 0) {
			return;
		}

		// First frame of a lyric: snap every line to its intensity so the effect does not ease in from nowhere.
		if (!this->initialized) {
			const animation::Easing easing = animation::resolveEasing(transitionTiming(anim).easing);
			for (::std::size_t i = 0; i < lineCount; ++i) {
				const int offset = static_cast<int>(i) - focus;
				this->scales[i].setEasing(easing);
				this->blurs[i].setEasing(easing);
				this->scales[i].snap(targetScale(effect, offset));
				this->blurs[i].snap(targetBlur(effect, offset));
			}
			this->focus       = focus;
			this->initialized = true;
			return;
		}

		// Active line changed: restart each line's ease with the delay its mode assigns (shared with the scroll cascade).
		if (focus != this->focus) {
			const animation::Easing easing    = animation::resolveEasing(transitionTiming(anim).easing);
			const int               direction = focus > this->focus ? 1 : (focus < this->focus ? -1 : 0);
			for (::std::size_t i = 0; i < lineCount; ++i) {
				const int        offset     = static_cast<int>(i) - focus;
				const Transition transition = lineTransition(anim, offset, direction);
				this->scales[i].setEasing(easing);
				this->blurs[i].setEasing(easing);
				this->scales[i].setDuration(transition.duration);
				this->blurs[i].setDuration(transition.duration);
				this->scales[i].retarget(now, targetScale(effect, offset), transition.delay);
				this->blurs[i].retarget(now, targetBlur(effect, offset), transition.delay);
			}
			this->focus = focus;
			return;
		}

		// Stable focus: follow config edits (enable / range changes) without restarting the eases.
		for (::std::size_t i = 0; i < lineCount; ++i) {
			const int offset = static_cast<int>(i) - focus;
			this->scales[i].setTarget(targetScale(effect, offset));
			this->blurs[i].setTarget(targetBlur(effect, offset));
		}
	}
} // namespace music_lyric_player::rendering::core
