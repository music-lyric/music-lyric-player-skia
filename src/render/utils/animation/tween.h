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
		    : from_(value),
		      to_(value) {}

		/**
		 * Sets the easing curve applied while animating; a tween with no easing behaves as settled.
		 */
		void setEasing(Easing easing) {
			easing_ = ::std::move(easing);
		}

		/**
		 * Sets the animation length in milliseconds; a value <= 0 disables easing (samples snap to `to`).
		 */
		void setDuration(double durationMs) {
			durationMs_ = durationMs;
		}

		/**
		 * Starts a fresh animation from the current sampled value towards `target`, beginning at `nowMs`.
		 */
		void retarget(double nowMs, const T& target) {
			from_    = sample(nowMs);
			to_      = target;
			startMs_ = nowMs;
		}

		/**
		 * Moves the landing value without restarting, so an in-flight ease keeps its progress and curve.
		 */
		void setTarget(const T& target) {
			to_ = target;
		}

		/**
		 * Settles instantly at `value` (`from` == `to`), so `sample` returns it with no animation.
		 */
		void snap(const T& value) {
			from_ = value;
			to_   = value;
		}

		/**
		 * Whether the animation has run past its duration at `nowMs`, or is disabled (`durationMs` <= 0).
		 */
		bool finished(double nowMs) const {
			return durationMs_ <= 0.0 || nowMs - startMs_ >= durationMs_;
		}

		/**
		 * Samples the eased value at `nowMs`; returns `to` once finished or when easing is unset / disabled.
		 */
		T sample(double nowMs) const {
			if (durationMs_ <= 0.0 || !easing_) {
				return to_;
			}
			const double elapsed = nowMs - startMs_;
			if (elapsed >= durationMs_) {
				return to_;
			}
			const float t = easing_(static_cast<float>(elapsed / durationMs_));
			return lerp(from_, to_, t);
		}

		/**
		 * The landing value the tween is easing towards.
		 */
		const T& target() const {
			return to_;
		}

	private:
		T      from_;
		T      to_;
		double startMs_    = 0.0;
		double durationMs_ = 0.0;
		Easing easing_;
	};
} // namespace music_lyric_player::render::animation

#endif // MUSIC_LYRIC_PLAYER_RENDER_UTILS_ANIMATION_TWEEN_H_
