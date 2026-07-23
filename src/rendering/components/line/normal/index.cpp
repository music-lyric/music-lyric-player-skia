#include "rendering/components/line/normal/index.h"

#include <algorithm>
#include <cmath>
#include <memory>

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkRect.h"
#include "rendering/common/context.h"
#include "rendering/components/line/normal/main/plain/index.h"
#include "rendering/components/line/normal/main/syllable/index.h"
#include "rendering/utils/animation/easing.h"
#include "rendering/utils/color/parse.h"

namespace music_lyric_player::rendering::components::line::normal {
	namespace {
		// The base opacity fade length; mirrors the web `.normal` `transition: opacity 0.6s ease`.
		constexpr double kBaseFadeDurationMs = 600.0;
	} // namespace

	Element::Element(int index, const music_lyric_model::parsed::Line& info, bool isSyllable)
	    : base::Element(index),
	      info(info),
	      syllableEnable(isSyllable) {
		// The base opacity eases on every state change with CSS `ease`; unlike the word, it never snaps on activation.
		this->baseOpacity.setEasing(animation::CubicBezier{0.25f, 0.1f, 0.25f, 1.0f});
		this->baseOpacity.setDuration(kBaseFadeDurationMs);
	}

	Element::~Element() = default;

	void Element::selectBody(bool useSyllable) {
		if (useSyllable == this->syllableMode && (this->plainElement || this->syllableElement)) {
			return;
		}

		this->syllableMode = useSyllable;
		if (useSyllable) {
			this->plainElement.reset();
			this->syllableElement = std::make_unique<main::syllable::Element>(this->info);
		} else {
			this->syllableElement.reset();
			this->plainElement = std::make_unique<main::plain::Element>(this->info);
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

	void Element::paint(SkCanvas* canvas, float x, float y, double now, bool active, bool played, const common::RenderContext& context) const {
		const config::Root& cfg = context.config;

		// Resolve and ease the line-wide base opacity across the normal / active / played states.
		const auto& baseStyle = cfg.line.normal.base.style;
		const float goal      = static_cast<float>(active ? baseStyle.active.opacity : (played ? baseStyle.played.opacity : baseStyle.normal.opacity));
		if (!this->baseOpacityReady) {
			this->baseOpacity.snap(goal);
			this->baseOpacityReady = true;
		} else if (goal != this->baseOpacityGoal) {
			this->baseOpacity.retarget(now, goal);
		} else {
			// Stable state: track config edits without restarting an in-flight fade.
			this->baseOpacity.setTarget(goal);
		}
		this->baseOpacityGoal   = goal;
		const float lineOpacity = std::clamp(this->baseOpacity.sample(now), 0.0f, 1.0f);

		const SkColor normalColor = utils::color::resolve(cfg.line.normal.main.syllable.style.normal.color, config::Default.line.normal.main.syllable.style.normal.color);
		const SkColor activeColor = utils::color::resolve(cfg.line.normal.main.syllable.style.active.color, config::Default.line.normal.main.syllable.style.active.color);

		// Composite the whole line through one alpha layer so the base opacity multiplies onto its words, exactly as CSS opacity composites onto children; skip the layer when the line is fully opaque.
		const int  alpha    = static_cast<int>(std::lround(lineOpacity * 255.0f));
		const bool useLayer = alpha < 255;
		if (useLayer) {
			const SkRect bounds = SkRect::MakeXYWH(x, y, this->width, this->measuredHeight).makeOutset(4.0f, 4.0f);
			canvas->saveLayerAlpha(&bounds, static_cast<U8CPU>(alpha));
		}

		if (this->syllableElement) {
			this->syllableElement->paint(canvas, x, y, now, active, played, context);
		} else if (this->plainElement) {
			const SkColor normalStyle = utils::color::withOpacity(normalColor, cfg.line.normal.main.syllable.style.normal.opacity);
			const SkColor activeStyle = utils::color::withOpacity(activeColor, cfg.line.normal.main.syllable.style.active.opacity);
			this->plainElement->paint(canvas, x, y, this->stateColor(now, active, normalStyle, activeStyle));
		}

		if (useLayer) {
			canvas->restore();
		}
	}
} // namespace music_lyric_player::rendering::components::line::normal
