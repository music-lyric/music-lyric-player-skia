#include "render/components/line/normal/syllable/animation/mask.h"

#include <algorithm>
#include <cmath>

#include "include/core/SkBlendMode.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPoint.h"
#include "include/core/SkTileMode.h"
#include "include/effects/SkGradient.h"

namespace music_lyric_player::render::components::line::normal::syllable::animation {
	Mask::Mask(double start, double duration, std::size_t index, std::size_t count)
	    : start(start),
	      duration(std::max(duration, 0.0)),
	      wordIndex(index),
	      wordCount(count) {}

	float Mask::progress(double currentTime) const {
		if (this->duration <= 0.0) {
			return currentTime >= this->start ? 1.0f : 0.0f;
		}
		return static_cast<float>(std::clamp((currentTime - this->start) / this->duration, 0.0, 1.0));
	}

	float Mask::feather(float height, double normal, double first, double last) const {
		normal = std::isfinite(normal) ? std::clamp(normal, 0.0, 2.0) : 0.5;
		first  = std::isfinite(first) ? std::clamp(first, 0.0, 5.0) : 1.5;
		last   = std::isfinite(last) ? std::clamp(last, 0.0, 5.0) : 0.5;

		double multiplier = 1.0;
		if (this->wordIndex == 0) {
			multiplier = first;
		}
		if (this->wordIndex + 1 == this->wordCount) {
			multiplier = std::max(multiplier, last);
		}
		return static_cast<float>(std::max(height, 0.0f) * normal * multiplier);
	}

	void Mask::apply(SkCanvas* canvas, const SkRect& bounds, float progress, float feather) const {
		const float   edge = bounds.left() + bounds.width() * progress;
		const SkPoint points[2]{
			{edge - feather, bounds.top()},
			{edge + feather, bounds.top()}
                };
		const SkColor4f colors[2]{SkColors::kWhite, SkColors::kTransparent};

		SkPaint paint;
		paint.setBlendMode(SkBlendMode::kDstIn);
		paint.setShader(SkShaders::LinearGradient(points, {
									  {colors, SkTileMode::kClamp},
									  {}
                },
			nullptr));
		canvas->drawRect(bounds, paint);
	}
} // namespace music_lyric_player::render::components::line::normal::syllable::animation
