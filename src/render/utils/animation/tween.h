#ifndef MUSIC_LYRIC_PLAYER_RENDER_UTILS_ANIMATION_TWEEN_H_
#define MUSIC_LYRIC_PLAYER_RENDER_UTILS_ANIMATION_TWEEN_H_

#include <utility>

#include "render/utils/animation/easing.h"
#include "render/utils/animation/interpolate.h"

namespace music_lyric_player::render::animation {
	/**
	 * A single value that eases from `from` to `to` over `durationMs`, beginning at `startMs`.
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
		void setDuration(double durationMs) {
			this->durationMs = durationMs;
		}

		/**
		 * Starts a fresh animation from the current sampled value towards `target`, beginning at `now`.
		 */
		void retarget(double now, const T& target) {
			this->from    = sample(now);
			this->to      = target;
			this->startMs = now;
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
		 * Whether the animation has run past its duration at `now`, or is disabled (`durationMs` <= 0).
		 */
		bool finished(double now) const {
			return this->durationMs <= 0.0 || now - this->startMs >= this->durationMs;
		}

		/**
		 * Samples the eased value at `now`; returns `to` once finished or when easing is unset / disabled.
		 */
		T sample(double now) const {
			if (this->durationMs <= 0.0 || !this->easing) {
				return this->to;
			}
			const double elapsed = now - this->startMs;
			if (elapsed >= this->durationMs) {
				return this->to;
			}
			const float t = this->easing(static_cast<float>(elapsed / this->durationMs));
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
		double startMs    = 0.0;
		double durationMs = 0.0;
		Easing easing;
	};
} // namespace music_lyric_player::render::animation

#endif // MUSIC_LYRIC_PLAYER_RENDER_UTILS_ANIMATION_TWEEN_H_
