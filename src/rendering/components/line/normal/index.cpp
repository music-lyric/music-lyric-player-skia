#include "rendering/components/line/normal/index.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <string>

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkFont.h"
#include "include/core/SkRect.h"
#include "music_lyric_model.h"
#include "rendering/common/context.h"
#include "rendering/components/line/normal/annotation/index.h"
#include "rendering/components/line/normal/main/plain/index.h"
#include "rendering/components/line/normal/main/syllable/index.h"
#include "rendering/utils/animation/easing.h"
#include "rendering/utils/color/parse.h"
#include "rendering/utils/length.h"
#include "rendering/utils/shaping/font.h"

namespace music_lyric_player::rendering::components::line::normal {
	namespace {
		// The base opacity fade length; mirrors the web `.normal` `transition: opacity 0.6s ease`.
		constexpr double kBaseFadeDurationMs = 600.0;

		/**
		 * Converts a resolved language tag into the optional the model getters expect.
		 * An empty tag means no preference, so the getter picks the first available content.
		 */
		std::optional<std::string> toLanguage(const std::string& tag) {
			if (tag.empty()) {
				return std::nullopt;
			}
			return tag;
		}

		/**
		 * Rebuilds and lays out one annotation row for the current relayout.
		 * The row is dropped when hidden or empty, while its `%` font size resolves against `baseFontPx`.
		 */
		void layoutAnnotationRow(std::unique_ptr<annotation::Row>& row, bool show, const std::optional<std::string>& text, const config::common::FontConfig& font, const std::string& fallbackSize, float baseFontPx, float width, const common::RenderContext& context) {
			if (!show || !text || text->empty()) {
				row.reset();
				return;
			}

			row                 = std::make_unique<annotation::Row>(*text);
			const float  size   = static_cast<float>(std::max(resolveLength(font.size.value(), fallbackSize, baseFontPx), 1.0));
			const SkFont skFont = utils::shaping::buildBodyFont(context.fontMgr, font.family.value(), size);
			row->layout(width, context, skFont, context.config.layout.align.value());
		}
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
			this->mainHeight = this->syllableElement->height();
		} else if (this->plainElement) {
			this->plainElement->layout(this->width, context);
			this->mainHeight = this->plainElement->height();
		} else {
			this->mainHeight = 0.0f;
		}

		// Stack the optional romanization and translation sub-lines above and below the main content.
		const config::Root& cfg    = context.config;
		const auto&         normal = cfg.line.normal;
		const auto&         ann    = normal.annotation;

		const bool showRoman     = ann.visible.value() && ann.roman.visible.value();
		const bool showTranslate = ann.visible.value() && ann.translate.visible.value();

		// The line-base pixel size is the reference a `%` annotation size scales against, matching the web inheritance chain.
		const float        baseFontPx   = static_cast<float>(std::max(resolveLength(normal.base.font.size.value(), config::Default.line.normal.base.font.size.value()), 1.0));
		const std::string& fallbackSize = config::Default.line.normal.annotation.base.font.size.value();

		layoutAnnotationRow(this->romanRow, showRoman, showRoman ? music_lyric_model::parsed::getParsedLineRoman(this->info, toLanguage(ann.roman.language.value())) : std::nullopt, ann.roman.font, fallbackSize, baseFontPx, this->width, context);
		layoutAnnotationRow(this->translateRow, showTranslate, showTranslate ? music_lyric_model::parsed::getParsedLineTranslation(this->info, toLanguage(ann.translate.language.value())) : std::nullopt, ann.translate.font, fallbackSize, baseFontPx, this->width, context);

		this->romanHeight     = this->romanRow ? this->romanRow->height() : 0.0f;
		this->translateHeight = this->translateRow ? this->translateRow->height() : 0.0f;
		this->measuredHeight  = this->romanHeight + this->mainHeight + this->translateHeight;
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

		// Fixed web-default stack order: romanization on top, main line, translation beneath.
		const auto& ann = cfg.line.normal.annotation;
		if (this->romanRow) {
			this->romanRow->paint(canvas, x, y, now, active, played, ann.roman.style);
		}

		const float mainY = this->romanHeight;
		if (this->syllableElement) {
			this->syllableElement->paint(canvas, x, y + mainY, now, active, played, context);
		} else if (this->plainElement) {
			const SkColor playedColor = utils::color::resolve(cfg.line.normal.main.syllable.style.played.color, config::Default.line.normal.main.syllable.style.played.color);
			const SkColor normalStyle = utils::color::withOpacity(normalColor, cfg.line.normal.main.syllable.style.normal.opacity);
			const SkColor activeStyle = utils::color::withOpacity(activeColor, cfg.line.normal.main.syllable.style.active.opacity);
			// The played tint mirrors the web `.plain` `apply-line-style('main-syllable')` `[played]` variant so a sung plain line dims to the same level as the timed path.
			const SkColor playedStyle = utils::color::withOpacity(playedColor, cfg.line.normal.main.syllable.style.played.opacity);
			this->plainElement->paint(canvas, x, y + mainY, this->stateColor(now, active, normalStyle, activeStyle, played, playedStyle));
		}

		if (this->translateRow) {
			this->translateRow->paint(canvas, x, y + mainY + this->mainHeight, now, active, played, ann.translate.style);
		}

		if (useLayer) {
			canvas->restore();
		}
	}
} // namespace music_lyric_player::rendering::components::line::normal
