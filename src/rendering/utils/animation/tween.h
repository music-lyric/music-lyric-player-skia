#ifndef MUSIC_LYRIC_PLAYER_RENDERING_UTILS_ANIMATION_TWEEN_H_
#define MUSIC_LYRIC_PLAYER_RENDERING_UTILS_ANIMATION_TWEEN_H_

#include <utility>

#include "rendering/utils/animation/easing.h"
#include "rendering/utils/animation/interpolate.h"

namespace music_lyric_player::rendering::animation {
	/**
	 * A single value that eases from `from` to `to` over `duration`, beginning at `start`.
	 * Holds no clock: the caller samples it each frame with the current time.
	 * It is the imperative stand-in for one CSS `transition`, which the browser interpolated for free but immediate-mode Skia must sample.
	 */
	template <typename T>
	class Tween {
	public:
		/**
		 * Creates a settled tween holding `value` (`from` == `to`); `sample` returns it until retargeted.
		 */
		explicit Tween(T value = T{})
		    : from(value),
		      to(value) {}

		/**
		 * Sets the easing curve applied while animating; a tween with no easing behaves as settled.
		 */
		void setEasing(Easing easing) {
			this->easing = ::std::move(easing);
		}

		/**
		 * Sets the animation length in milliseconds; a value <= 0 disables easing (samples snap to `to`).
		 */
		void setDuration(double duration) {
			this->duration = duration;
		}

		/**
		 * Starts a fresh animation from the current sampled value towards `target`, beginning at `now` plus `delay`.
		 * A positive `delay` holds the current value until the delay elapses, mirroring a CSS `transition-delay`.
		 */
		void retarget(double now, const T& target, double delay = 0.0) {
			this->from  = sample(now);
			this->to    = target;
			this->start = now + (delay > 0.0 ? delay : 0.0);
		}

		/**
		 * Moves the landing value without restarting, so an in-flight ease keeps its progress and curve.
		 */
		void setTarget(const T& target) {
			this->to = target;
		}

		/**
		 * Settles instantly at `value` (`from` == `to`), so `sample` returns it with no animation.
		 */
		void snap(const T& value) {
			this->from = value;
			this->to   = value;
		}

		/**
		 * Whether the animation has run past its duration at `now`, or is disabled (`duration` <= 0).
		 */
		bool finished(double now) const {
			return this->duration <= 0.0 || now - this->start >= this->duration;
		}

		/**
		 * Samples the eased value at `now`; returns `to` once finished or when easing is unset / disabled.
		 */
		T sample(double now) const {
			if (this->duration <= 0.0 || !this->easing) {
				return this->to;
			}
			const double elapsed = now - this->start;
			if (elapsed >= this->duration) {
				return this->to;
			}
			const float t = this->easing(static_cast<float>(elapsed / this->duration));
			return lerp(this->from, this->to, t);
		}

		/**
		 * The landing value the tween is easing towards.
		 */
		const T& target() const {
			return this->to;
		}

	private:
		T      from;
		T      to;
		double start    = 0.0;
		double duration = 0.0;
		Easing easing;
	};
} // namespace music_lyric_player::rendering::animation

#endif // MUSIC_LYRIC_PLAYER_RENDERING_UTILS_ANIMATION_TWEEN_H_
