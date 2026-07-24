#include "rendering/core/transition.h"

#include <algorithm>
#include <cmath>

#include "rendering/config/scroll/index.gen.h"

namespace music_lyric_player::rendering::core {
	namespace {
		// Local alias for the config cascade-mode enum, so the switch below reads without the full path.
		using Mode = config::scroll::Mode;
	} // namespace

	TransitionTiming transitionTiming(const config::scroll::AnimationConfig& anim) {
		switch (anim.mode) {
		case Mode::Ripple:
			return {anim.ripple.duration, anim.ripple.easing};
		case Mode::Directional:
			return {anim.directional.duration, anim.directional.easing};
		case Mode::Stagger:
			return {anim.stagger.duration, anim.stagger.easing};
		case Mode::Smooth:
		default:
			return {anim.smooth.duration, anim.smooth.easing};
		}
	}

	Transition lineTransition(const config::scroll::AnimationConfig& anim, int offset, int direction) {
		const double duration = ::std::max(transitionTiming(anim).duration, 0.0);
		switch (anim.mode) {
		case Mode::Ripple: {
			const double step       = ::std::max(anim.ripple.step.value(), 10.0);
			const double range      = ::std::max(anim.ripple.range.value(), 1.0);
			const double distance   = ::std::min(::std::abs(static_cast<double>(offset)), range);
			const double normalized = distance / range;
			const double eased      = 1.0 - (1.0 - normalized) * (1.0 - normalized);
			return {duration, ::std::round(eased * range * step)};
		}
		case Mode::Directional: {
			const double step       = ::std::max(anim.directional.step.value(), 10.0);
			const double range      = ::std::max(anim.directional.range.value(), 1.0);
			const double distance   = ::std::min(::std::abs(static_cast<double>(offset)), range);
			const double normalized = distance / range;
			const bool   trailing   = direction != 0 && offset * direction < 0;
			const double eased      = trailing
				     ? (1.0 - normalized) * (1.0 - normalized)
				     : 1.0 - (1.0 - normalized) * (1.0 - normalized);
			return {duration, ::std::round(eased * range * step)};
		}
		case Mode::Stagger: {
			if (direction == 0) {
				return {duration, 0.0};
			}
			const double range   = ::std::max(anim.stagger.range.value(), 1.0);
			const double step    = ::std::max(anim.stagger.step.value(), 1.0);
			const double clamped = ::std::max(-range, ::std::min(range, static_cast<double>(offset)));
			return {duration, ::std::round((range + direction * clamped) * step)};
		}
		case Mode::Smooth:
		default:
			return {duration, ::std::max(anim.smooth.delay.value(), 0.0)};
		}
	}
} // namespace music_lyric_player::rendering::core
