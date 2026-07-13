#include "render/components/line/normal/main/syllable/animation/mask.h"

#include <algorithm>
#include <cmath>

#include "include/core/SkBlendMode.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPoint.h"
#include "include/core/SkTileMode.h"
#include "include/effects/SkGradient.h"

namespace music_lyric_player::render::components::line::normal::main::syllable::animation {
	Mask::Mask(double lineStart, double lineDuration)
	    : lineStart(lineStart),
	      lineDuration(std::max(lineDuration, 0.0)) {}

	void Mask::update(const std::vector<Input>& inputs) {
		this->segments.clear();
		this->segments.reserve(inputs.size());
		this->phases.assign(inputs.size(), 0.0f);
		this->frames.assign(inputs.size(), Frame{});
		if (this->lineDuration <= 0.0) {
			for (Frame& frame : this->frames) {
				frame.progress = 1.0f;
			}
			return;
		}

		double timeline = 0.0;
		double offset   = this->lineStart;
		float  prefix   = 0.0f;
		for (const Input& source : inputs) {
			Input input    = source;
			input.start    = std::isfinite(input.start) ? input.start : this->lineStart;
			input.duration = std::isfinite(input.duration) ? std::max(input.duration, 0.0) : 0.0;
			input.width    = std::isfinite(input.width) ? std::max(input.width, 0.0f) : 0.0f;
			input.height   = std::isfinite(input.height) ? std::max(input.height, 0.0f) : 0.0f;

			const double gap = input.start - offset;
			if (gap > 0.0) {
				timeline += gap;
			}
			const double moveStart = std::min(timeline, this->lineDuration);
			timeline += input.duration;
			const double moveEnd = std::min(timeline, this->lineDuration);
			this->segments.push_back(Segment{input, moveStart, std::max(moveEnd - moveStart, 0.0), prefix});
			offset = input.start + input.duration;
			prefix += input.width;
		}
	}

	void Mask::sample(double currentTime, double normal, double first, double last) const {
		if (this->segments.empty()) {
			return;
		}

		normal                    = std::isfinite(normal) ? std::clamp(normal, 0.0, 2.0) : 0.5;
		first                     = std::isfinite(first) ? std::clamp(first, 0.0, 5.0) : 1.5;
		last                      = std::isfinite(last) ? std::clamp(last, 0.0, 5.0) : 0.5;
		const double relativeTime = currentTime - this->lineStart;

		float advance = 0.0f;
		for (std::size_t i = 0; i < this->segments.size(); ++i) {
			const Segment& segment  = this->segments[i];
			float          progress = 0.0f;
			if (segment.moveDuration <= 0.0) {
				progress = relativeTime >= segment.moveStart ? 1.0f : 0.0f;
			} else {
				progress = static_cast<float>(std::clamp((relativeTime - segment.moveStart) / segment.moveDuration, 0.0, 1.0));
			}
			this->phases[i] = progress;
			advance += segment.input.width * progress;
		}

		const float firstProgress = this->phases.front();
		const float lastProgress  = this->phases.back();
		for (std::size_t i = 0; i < this->segments.size(); ++i) {
			const Segment& segment = this->segments[i];
			Frame&         frame   = this->frames[i];
			if (segment.input.width <= 0.0f) {
				frame = Frame{};
				continue;
			}

			const float feather     = segment.input.height * static_cast<float>(normal);
			const float positionMin = -(segment.input.width + feather);
			const float cursor      = -segment.prefix - segment.input.width - 2.0f * feather + advance + feather * static_cast<float>(first) * firstProgress + feather * static_cast<float>(last) * lastProgress;
			const float position    = std::clamp(cursor, positionMin, 0.0f);
			frame.progress          = (position - positionMin) / -positionMin;
			frame.feather           = feather;
		}
	}

	float Mask::progress(std::size_t index) const {
		return index < this->frames.size() ? this->frames[index].progress : 0.0f;
	}

	float Mask::feather(std::size_t index) const {
		return index < this->frames.size() ? this->frames[index].feather : 0.0f;
	}

	void Mask::apply(SkCanvas* canvas, const SkRect& drawBounds, const SkRect& textBounds, float progress, float feather, SkColor unsungColor, SkColor sungColor) {
		progress = std::clamp(progress, 0.0f, 1.0f);
		feather  = std::max(feather, 0.0f);

		SkPaint paint;
		paint.setBlendMode(SkBlendMode::kSrcIn);
		if (feather <= 0.0f) {
			paint.setColor(unsungColor);
			canvas->drawRect(drawBounds, paint);

			canvas->save();
			canvas->clipRect(SkRect::MakeLTRB(
				drawBounds.left(),
				drawBounds.top(),
				textBounds.left() + textBounds.width() * progress,
				drawBounds.bottom()));
			paint.setColor(sungColor);
			canvas->drawRect(drawBounds, paint);
			canvas->restore();
			return;
		}

		// The transition band is exactly one feather wide and slides so its edges meet the word box, matching the web mask geometry.
		const float transitionStart = textBounds.left() - feather + progress * (textBounds.width() + feather);
		const SkPoint points[2]{
			{transitionStart, textBounds.top()},
			{transitionStart + feather, textBounds.top()}
		};
		const SkColor4f colors[2]{SkColor4f::FromColor(sungColor), SkColor4f::FromColor(unsungColor)};
		const SkGradient::Interpolation interpolation{
			SkGradient::Interpolation::InPremul::kNo,
			SkGradient::Interpolation::ColorSpace::kSRGB,
			SkGradient::Interpolation::HueMethod::kShorter
		};
		paint.setShader(SkShaders::LinearGradient(points, {{colors, SkTileMode::kClamp}, interpolation}, nullptr));
		canvas->drawRect(drawBounds, paint);
	}
} // namespace music_lyric_player::render::components::line::normal::main::syllable::animation
