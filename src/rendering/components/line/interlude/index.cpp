#include "rendering/components/line/interlude/index.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkPaint.h"
#include "music_lyric_model.h"
#include "rendering/common/context.h"
#include "rendering/utils/animation/easing.h"
#include "rendering/utils/animation/interpolate.h"
#include "rendering/utils/color/parse.h"
#include "rendering/utils/length.h"

namespace music_lyric_player::rendering::components::line::interlude {
	namespace {
		constexpr float kDotGap   = 12.0f;
		constexpr float kPaddingY = 6.0f;
	} // namespace

	Element::Element(int index, const music_lyric_model::parsed::Line& info)
	    : base::Element(index) {
		const music_lyric_model::common::Time* time = music_lyric_model::parsed::getParsedLineTime(info);
		this->start                                = time ? static_cast<double>(time->start) : 0.0;
		const double duration                      = static_cast<double>(music_lyric_model::parsed::getParsedLineDuration(info));
		this->slice                       = std::floor(std::max(duration, 0.0) / static_cast<double>(kDotCount));

		this->scaleTween.setDuration(kScaleDuration);
		this->scaleTween.setEasing(animation::CubicBezier(0.0f, 0.0f, 0.58f, 1.0f));
		for (animation::Tween<float>& tween : this->opacityTweens) {
			tween.setDuration(kOpacityFadeDuration);
			tween.setEasing(animation::linear);
		}
	}

	void Element::layout(float width, const common::RenderContext& context) {
		this->width          = std::max(width, 1.0f);
		this->dotSize        = static_cast<float>(std::max(resolveLength(context.config.line.interlude.size, config::Default.line.interlude.size), 0.0));
		this->measuredHeight = this->dotSize + 2.0f * kPaddingY;
	}

	float Element::stateScale(double now, bool active) const {
		if (active != this->animationActive) {
			this->scaleTween.setDuration(kScaleDuration);
			this->scaleTween.retarget(now, active ? kActiveScale : kInactiveScale, active ? kScaleActivationDelay : 0.0);
		}
		return this->scaleTween.sample(now);
	}

	float Element::dotOpacity(std::size_t index, double now, double currentTime, bool active, bool deactivating, float normalOpacity, float activeOpacity) const {
		normalOpacity = std::clamp(normalOpacity, 0.0f, 1.0f);
		activeOpacity = std::clamp(activeOpacity, 0.0f, 1.0f);

		animation::Tween<float>& tween = this->opacityTweens[index];
		if (active) {
			float value = activeOpacity;
			if (this->slice > 0.0) {
				const double delay    = this->slice * static_cast<double>(index);
				const double elapsed  = currentTime - this->start - delay;
				const float  progress = static_cast<float>(std::clamp(elapsed / this->slice, 0.0, 1.0));
				value                 = animation::lerp(normalOpacity, activeOpacity, progress);
			}
			tween.snap(value);
			return value;
		}

		if (!this->opacityReady) {
			tween.snap(normalOpacity);
		} else if (deactivating) {
			tween.setDuration(kOpacityFadeDuration);
			tween.retarget(now, normalOpacity);
		} else {
			tween.setTarget(normalOpacity);
		}
		return tween.sample(now);
	}

	void Element::paint(SkCanvas* canvas, float x, float y, double now, bool active, bool /* played */, const common::RenderContext& context) const {
		const config::Root& cfg   = context.config;
		const SkColor       color = this->stateColor(
                        now,
                        active,
                        utils::color::resolve(cfg.line.interlude.style.normal.color, config::Default.line.interlude.style.normal.color),
                        utils::color::resolve(cfg.line.interlude.style.active.color, config::Default.line.interlude.style.active.color));
		const double normalOpacityValue = std::isfinite(cfg.line.interlude.style.normal.opacity)
			? cfg.line.interlude.style.normal.opacity
			: config::Default.line.interlude.style.normal.opacity;
		const double activeOpacityValue = std::isfinite(cfg.line.interlude.style.active.opacity)
			? cfg.line.interlude.style.active.opacity
			: config::Default.line.interlude.style.active.opacity;
		const float  normalOpacity      = static_cast<float>(normalOpacityValue);
		const float  activeOpacity      = static_cast<float>(activeOpacityValue);
		const bool   deactivating       = !active && this->animationActive;
		const float  scale              = this->stateScale(now, active);

		const float radius   = this->dotSize * 0.5f;
		const float cy       = y + kPaddingY + radius;
		const float rowWidth = static_cast<float>(kDotCount) * this->dotSize + static_cast<float>(kDotCount - 1) * kDotGap;
		const float startX   = this->alignBlockX(x, rowWidth, cfg.layout.align);

		float originX = startX;
		switch (cfg.layout.align) {
		case config::layout::Align::Center:
			originX += rowWidth * 0.5f;
			break;
		case config::layout::Align::Right:
			originX += rowWidth;
			break;
		case config::layout::Align::Left:
		default:
			break;
		}

		canvas->save();
		canvas->translate(originX, cy);
		canvas->scale(scale, scale);
		canvas->translate(-originX, -cy);

		SkPaint paint;
		paint.setAntiAlias(true);
		for (std::size_t i = 0; i < kDotCount; ++i) {
			const float opacity = this->dotOpacity(i, now, context.currentTime, active, deactivating, normalOpacity, activeOpacity);
			const float alpha   = std::clamp(opacity * static_cast<float>(SkColorGetA(color)), 0.0f, 255.0f);
			paint.setColor(SkColorSetA(color, static_cast<std::uint8_t>(std::lround(alpha))));

			const float cx = startX + radius + static_cast<float>(i) * (this->dotSize + kDotGap);
			canvas->drawCircle(cx, cy, radius, paint);
		}
		canvas->restore();

		this->opacityReady    = true;
		this->animationActive = active;
	}
} // namespace music_lyric_player::rendering::components::line::interlude
