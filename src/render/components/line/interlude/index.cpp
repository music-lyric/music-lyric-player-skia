#include "render/components/line/interlude/index.h"

#include <algorithm>

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkPaint.h"
#include "render/common/context.h"

namespace music_lyric_player::render::components::line::interlude {
	namespace {
		// Fixed dot-row geometry in logical pixels; the interlude style config is not ported yet (M2.3+).
		constexpr float kDotSize  = 16.0f;
		constexpr float kDotGap   = 12.0f;
		constexpr float kPaddingY = 6.0f;
	} // namespace

	Element::Element(int index)
	    : base::Element(index) {}

	void Element::layout(float width, const common::RenderContext& /*context*/) {
		this->width          = std::max(width, 1.0f);
		this->measuredHeight = kDotSize + 2.0f * kPaddingY;
	}

	void Element::paint(SkCanvas* canvas, float x, float y, double now, bool active, const common::RenderContext& context) const {
		const config::Root& cfg   = context.config;
		const SkColor       color = this->stateColor(now, active, static_cast<SkColor>(cfg.line.normal.color), static_cast<SkColor>(cfg.line.active.color));

		SkPaint paint;
		paint.setAntiAlias(true);
		paint.setColor(color);

		const float radius = kDotSize * 0.5f;
		const float cy     = y + kPaddingY + radius;
		// The whole dot row is a rigid block; base places it per `layout.align`, like a normal line's text.
		const float rowWidth = kDotCount * kDotSize + (kDotCount - 1) * kDotGap;
		const float startX   = this->alignBlockX(x, rowWidth, cfg.layout.align);
		for (int i = 0; i < kDotCount; ++i) {
			const float cx = startX + radius + static_cast<float>(i) * (kDotSize + kDotGap);
			canvas->drawCircle(cx, cy, radius, paint);
		}
	}
} // namespace music_lyric_player::render::components::line::interlude
