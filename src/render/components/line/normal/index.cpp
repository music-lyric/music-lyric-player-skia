#include "render/components/line/normal/index.h"

#include <algorithm>
#include <memory>

#include "include/core/SkColor.h"
#include "render/common/context.h"
#include "render/components/line/normal/plain/index.h"
#include "render/utils/color/parse.h"

namespace music_lyric_player::render::components::line::normal {
	Element::Element(int index, const ::lyric::runtime::Line& info)
	    : base::Element(index),
	      plainElement(std::make_unique<plain::Element>(info)) {}

	Element::~Element() = default;

	void Element::layout(float width, const common::RenderContext& context) {
		this->width = std::max(width, 1.0f);
		this->plainElement->layout(this->width, context);
		this->measuredHeight = this->plainElement->height();
	}

	void Element::paint(SkCanvas* canvas, float x, float y, double now, bool active, const common::RenderContext& context) const {
		const config::Root& cfg   = context.config;
		const SkColor       color = this->stateColor(now, active, utils::color::resolve(cfg.line.normal.color, config::Default.line.normal.color), utils::color::resolve(cfg.line.active.color, config::Default.line.active.color));
		this->plainElement->paint(canvas, x, y, color);
	}
} // namespace music_lyric_player::render::components::line::normal
