#include "render/core/transition.h"

#include <algorithm>
#include <cmath>

#include "render/config/scroll/index.gen.h"

namespace music_lyric_player::render::core {
	namespace {
		// Local alias for the config cascade-mode enum, so the switch below reads without the full path.
		using Mode = config::scroll::Mode;
	} // namespace

	Transition lineTransition(const config::scroll::AnimationConfig& anim, int offset, bool played, int direction) {
		const double duration = ::std::max(anim.duration, 0.0);
		switch (anim.mode) {
		case Mode::Ripple: {
			const double step       = ::std::max(anim.ripple.step, 10.0);
			const double range      = ::std::max(anim.ripple.range, 1.0);
			const double distance   = ::std::min(::std::abs(static_cast<double>(offset)), range);
			const double normalized = distance / range;
			const double eased      = 1.0 - (1.0 - normalized) * (1.0 - normalized);
			return {duration, ::std::round(eased * range * step)};
		}
		case Mode::Directional: {
			const double step       = ::std::max(anim.directional.step, 10.0);
			const double range      = ::std::max(anim.directional.range, 1.0);
			const double distance   = ::std::min(::std::abs(static_cast<double>(offset)), range);
			const double normalized = distance / range;
			const double eased      = played
			         ? (1.0 - normalized) * (1.0 - normalized)
			         : 1.0 - (1.0 - normalized) * (1.0 - normalized);
			return {duration, ::std::round(eased * range * step)};
		}
		case Mode::Stagger: {
			if (direction == 0) {
				return {duration, 0.0};
			}
			const double range   = ::std::max(anim.stagger.range, 1.0);
			const double step    = ::std::max(anim.stagger.step, 1.0);
			const double clamped = ::std::max(-range, ::std::min(range, static_cast<double>(offset)));
			return {duration, ::std::round((range + direction * clamped) * step)};
		}
		case Mode::Smooth:
		default:
			return {duration, ::std::max(anim.smooth.delay, 0.0)};
		}
	}
} // namespace music_lyric_player::render::core
