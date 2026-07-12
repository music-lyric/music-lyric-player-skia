#include "render/components/line/normal/index.h"

#include <algorithm>
#include <memory>

#include "include/core/SkColor.h"
#include "render/common/context.h"
#include "render/components/line/normal/plain/index.h"
#include "render/components/line/normal/syllable/index.h"
#include "render/utils/color/parse.h"

namespace music_lyric_player::render::components::line::normal {
	Element::Element(int index, const ::lyric::runtime::Line& info, bool isSyllable)
	    : base::Element(index),
	      info(info),
	      syllableEnable(isSyllable) {}

	Element::~Element() = default;

	void Element::selectBody(bool useSyllable) {
		if (useSyllable == this->syllableMode && (this->plainElement || this->syllableElement)) {
			return;
		}

		this->syllableMode = useSyllable;
		if (useSyllable) {
			this->plainElement.reset();
			this->syllableElement = std::make_unique<syllable::Element>(this->info);
		} else {
			this->syllableElement.reset();
			this->plainElement = std::make_unique<plain::Element>(this->info);
		}
	}

	void Element::layout(float width, const common::RenderContext& context) {
		using Use = config::line::normal::main::Use;

		this->width = std::max(width, 1.0f);
		this->selectBody(this->syllableEnable && context.config.line.normal.main.use == Use::Syllable);
		if (this->syllableElement) {
			this->syllableElement->layout(this->width, context);
			this->measuredHeight = this->syllableElement->height();
		} else if (this->plainElement) {
			this->plainElement->layout(this->width, context);
			this->measuredHeight = this->plainElement->height();
		} else {
			this->measuredHeight = 0.0f;
		}
	}

	void Element::paint(SkCanvas* canvas, float x, float y, double now, bool active, const common::RenderContext& context) const {
		const config::Root& cfg         = context.config;
		const SkColor       normalColor = utils::color::resolve(cfg.line.style.normal.color, config::Default.line.style.normal.color);
		const SkColor       activeColor = utils::color::resolve(cfg.line.style.active.color, config::Default.line.style.active.color);

		if (this->syllableElement) {
			this->syllableElement->paint(canvas, x, y, now, active, context);
		} else if (this->plainElement) {
			const SkColor normalStyle = utils::color::withOpacity(normalColor, cfg.line.style.normal.opacity);
			const SkColor activeStyle = utils::color::withOpacity(activeColor, cfg.line.style.active.opacity);
			this->plainElement->paint(canvas, x, y, this->stateColor(now, active, normalStyle, activeStyle));
		}
	}
} // namespace music_lyric_player::render::components::line::normal
