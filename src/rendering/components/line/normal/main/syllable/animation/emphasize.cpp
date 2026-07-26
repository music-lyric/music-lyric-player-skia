#include "rendering/components/line/normal/main/syllable/animation/emphasize.h"

#include <algorithm>
#include <cmath>

namespace music_lyric_player::rendering::components::line::normal::main::syllable::animation {
	namespace {
		/**
		 * Maps the stretched syllable duration to the main pulse intensity: soft cubic below the 2s pivot, square root above it, scaled and clamped like the web.
		 */
		float mainIntensity(double raw) {
			const double ratio  = raw / 2000.0;
			const double shaped = ratio > 1.0 ? std::sqrt(ratio) : ratio * ratio * ratio;
			return static_cast<float>(std::clamp(shaped * 0.6, 0.0, 1.2));
		}

		/**
		 * Evaluates the triangle pulse factor at clamped progress `p`: rest to peak over the first half with the rise easing, peak back to rest over the second half with the fall easing.
		 */
		float pulse(float p, const ::music_lyric_player::rendering::animation::Easing& rise, const ::music_lyric_player::rendering::animation::Easing& fall) {
			if (p <= 0.0f || p >= 1.0f) {
				return 0.0f;
			}
			if (p <= 0.5f) {
				return rise(p * 2.0f);
			}
			return 1.0f - fall(p * 2.0f - 1.0f);
		}
	} // namespace

	const ::music_lyric_player::rendering::animation::Easing& Emphasize::CachedEasing::resolve(const std::string& value) {
		if (!this->ready || this->key != value) {
			this->key   = value;
			this->fn    = ::music_lyric_player::rendering::animation::resolveEasing(value);
			this->ready = true;
		}
		return this->fn;
	}

	Emphasize::Emphasize(double start, double duration)
	    : start(std::isfinite(start) ? start : 0.0),
	      duration(std::isfinite(duration) ? std::max(duration, 0.0) : 0.0) {}

	const std::vector<Emphasize::Transform>& Emphasize::sample(double currentTime, double now, bool active, double minDuration, double disableRate, std::size_t cellCount, const MainSettings& main) const {
		this->transforms.assign(cellCount, Transform{});

		// Web params: the timeline lasts at least minDuration and cells step by a fixed share of it.
		const double raw = std::max(std::max(minDuration, 0.0), this->duration);
		if (cellCount == 0 || raw <= 0.0) {
			this->lastActive = active;
			this->exiting    = false;
			return this->transforms;
		}
		const double stagger = raw / 2.5 / static_cast<double>(cellCount);

		if (active) {
			this->lastActive = true;
			this->exiting    = false;
			this->lastTime   = std::isfinite(currentTime) ? currentTime : 0.0;
		} else {
			// The line just deactivated: fast-forward any in-flight pulse from the last active frame instead of reversing it.
			if (this->lastActive) {
				this->lastActive = false;
				this->exiting    = true;
				this->exitBase   = this->lastTime;
				this->exitStart  = std::isfinite(now) ? now : 0.0;
			}
			if (!this->exiting) {
				return this->transforms;
			}
		}

		// The wind-down advances on wall-clock time at the clamped disable rate, so it keeps settling even while playback is paused.
		const double base    = active ? this->lastTime : this->exitBase;
		const double elapsed = active ? 0.0 : std::max(std::isfinite(now) ? now - this->exitStart : 0.0, 0.0) * std::max(disableRate, 1.0);

		bool residual = false;
		if (main.enabled) {
			const ::music_lyric_player::rendering::animation::Easing& rise = this->mainRise.resolve(main.easingRise);
			const ::music_lyric_player::rendering::animation::Easing& fall = this->mainFall.resolve(main.easingFall);

			const float intensity = mainIntensity(raw);
			const float peakScale = main.scale * intensity;
			const float peakDx    = main.offsetHorizontal * intensity;
			const float peakDy    = -main.offsetVertical * intensity;

			for (std::size_t i = 0; i < cellCount; ++i) {
				const double begun = (base - this->start - stagger * static_cast<double>(i)) / raw;
				// A cell that never started before deactivation holds rest through the wind-down.
				if (!active && begun <= 0.0) {
					continue;
				}
				const double progress = std::clamp(begun + elapsed / raw, 0.0, 1.0);
				if (!active && progress < 1.0) {
					residual = true;
				}
				const float factor = pulse(static_cast<float>(progress), rise, fall);
				if (factor <= 0.0f) {
					continue;
				}
				// Characters fan outward from the word center, so the middle cell stays put while the edges spread the most.
				const float spread = static_cast<float>(cellCount) * 0.5f - static_cast<float>(i);
				Transform&  cell   = this->transforms[i];
				cell.scale = 1.0f + peakScale * factor;
				cell.dx    = -peakDx * spread * factor;
				cell.dy    = peakDy * factor;
			}
		}
		if (!active && !residual) {
			this->exiting = false;
		}
		return this->transforms;
	}
} // namespace music_lyric_player::rendering::components::line::normal::main::syllable::animation
