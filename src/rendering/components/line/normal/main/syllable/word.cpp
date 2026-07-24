#include "rendering/components/line/normal/main/syllable/word.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkFont.h"
#include "include/core/SkRect.h"
#include "music_lyric_model.h"
#include "rendering/common/context.h"
#include "rendering/utils/color/parse.h"
#include "rendering/utils/fragment/builder.h"
#include "rendering/utils/length.h"
#include "rendering/utils/shaping/font.h"
#include "rendering/utils/shaping/shaper.h"

namespace music_lyric_player::rendering::components::line::normal::main::syllable {
	namespace {
		constexpr float kGlyphOutset    = 2.0f;
		constexpr float kUnboundedWidth = 1'000'000.0f;
		constexpr float kWidthEpsilon   = 0.01f;

		/**
		 * Returns the word's absolute start time, or zero when timing is absent.
		 */
		double wordStart(const music_lyric_model::common::WordNormal& info) {
			const music_lyric_model::common::Time* time = music_lyric_model::common::getWordTime(info);
			return time ? static_cast<double>(time->start) : 0.0;
		}

		/**
		 * Returns the word's non-negative duration.
		 */
		double wordDuration(const music_lyric_model::common::WordNormal& info) {
			return std::max(static_cast<double>(music_lyric_model::common::getWordDuration(info)), 0.0);
		}
	} // namespace

	Word::Word(const music_lyric_model::common::WordNormal& info, bool hasSpaceBefore)
	    : text(info.content),
	      start(wordStart(info)),
	      duration(wordDuration(info)),
	      spaceBefore(hasSpaceBefore),
	      floating(this->start, this->duration) {}

	Word::~Word() = default;

	void Word::layout(const common::RenderContext& context) {
		this->measuredWidth    = 0.0f;
		this->measuredHeight   = 0.0f;
		this->measuredBaseline = 0.0f;
		this->group            = {};

		if (!context.shaper || !context.fontMgr || this->text.empty()) {
			return;
		}

		const config::Root& cfg  = context.config;
		const float         size = static_cast<float>(std::max(resolveLength(cfg.line.normal.main.syllable.font.size, config::Default.line.normal.main.syllable.font.size), 1.0));
		const SkFont        font = utils::shaping::buildBodyFont(context.fontMgr, cfg.line.normal.main.syllable.font.family.value(), size);

		const char*       utf8  = this->text.c_str();
		const std::size_t bytes = this->text.size();

		const utils::shaping::ShapedText shaped = utils::shaping::shapeText(*context.shaper, context.unicode, context.fontMgr, font, utf8, bytes, kUnboundedWidth);
		// The word is shaped unbounded, so makeTextGroup collapses it to one fragment; ceil/epsilon stay component-local for row packing.
		this->group            = utils::fragment::makeTextGroup(shaped, utf8);
		if (!this->group) {
			return;
		}
		this->measuredWidth    = std::ceil(std::max(this->group.advance, 1.0f) + kWidthEpsilon);
		this->measuredBaseline = this->group.ascent;
		this->measuredHeight   = this->group.height;
	}

	void Word::setPosition(float x, float y) {
		this->x = x;
		this->y = y;
	}

	void Word::paintGroup(SkCanvas* canvas, float x, float y, SkColor color) const {
		this->group.paint(canvas, x, y, color);
	}

	void Word::paintReveal(SkCanvas* canvas, float x, float y, float progress, float feather, SkColor unsungColor, SkColor sungColor) const {
		if (!this->group) {
			return;
		}

		// Use the ceil'd layout box (not group.bounds) so the mask saveLayer size stays bit-identical to today.
		const SkRect textBounds = SkRect::MakeXYWH(x, y, this->measuredWidth, this->measuredHeight);
		const SkRect drawBounds = textBounds.makeOutset(kGlyphOutset, kGlyphOutset);
		canvas->saveLayer(&drawBounds, nullptr);
		// Opaque coverage: the mask supplies the alpha, so the base glyph layer stays fully opaque here.
		this->paintGroup(canvas, x, y, SK_ColorWHITE);
		animation::Mask::apply(canvas, drawBounds, textBounds, progress, feather, unsungColor, sungColor);
		canvas->restore();
	}

	void Word::paint(SkCanvas* canvas, float lineX, float lineY, double now, bool active, bool maskEnabled, float maskProgress, float maskFeather, float inactiveOpacity, const common::RenderContext& context) const {
		if (!this->group) {
			return;
		}

		const config::Root& cfg             = context.config;
		const auto&         animationConfig = cfg.line.normal.main.syllable.word.animation;
		const double        fromValue       = std::isfinite(animationConfig.floating.from)
			      ? animationConfig.floating.from
			      : config::Default.line.normal.main.syllable.word.animation.floating.from;
		const double        toValue         = std::isfinite(animationConfig.floating.to)
			      ? animationConfig.floating.to
			      : config::Default.line.normal.main.syllable.word.animation.floating.to;
		const float         offset          = this->floating.sample(context.currentTime, now, active, animationConfig.floating.enabled, static_cast<float>(fromValue), static_cast<float>(toValue));
		const float         drawX           = lineX + this->x;
		const float         drawY           = lineY + this->y + offset;

		const SkColor normalColor    = utils::color::resolve(cfg.line.normal.main.syllable.style.normal.color, config::Default.line.normal.main.syllable.style.normal.color);
		const SkColor activeColor    = utils::color::resolve(cfg.line.normal.main.syllable.style.active.color, config::Default.line.normal.main.syllable.style.active.color);
		const double  normalOpacity  = cfg.line.normal.main.syllable.style.normal.opacity;
		const double  activeOpacity  = cfg.line.normal.main.syllable.style.active.opacity;

		// Inactive lines paint the whole word in the normal state color; the opacity is eased by the owning element so a deactivating line fades out (web `.word` `transition: opacity 0.8s ease`) instead of snapping.
		if (!active) {
			this->paintGroup(canvas, drawX, drawY, utils::color::withOpacity(normalColor, inactiveOpacity));
			return;
		}

		// The active line switches the rgb to the active color at once; the mask only wipes alpha, from the normal opacity (unsung) up to the active opacity (sung), so the hue never changes.
		const SkColor sungColor   = utils::color::withOpacity(activeColor, activeOpacity);
		const SkColor unsungColor = utils::color::withOpacity(activeColor, normalOpacity);

		if (!maskEnabled || maskProgress >= 1.0f) {
			this->paintGroup(canvas, drawX, drawY, sungColor);
			return;
		}
		if (maskProgress <= 0.0f) {
			this->paintGroup(canvas, drawX, drawY, unsungColor);
			return;
		}
		this->paintReveal(canvas, drawX, drawY, maskProgress, maskFeather, unsungColor, sungColor);
	}

	animation::Mask::Input Word::maskInput() const {
		return animation::Mask::Input{this->start, this->duration, this->measuredWidth, this->measuredHeight};
	}

	bool Word::hasSpaceBefore() const {
		return this->spaceBefore;
	}

	float Word::width() const {
		return this->measuredWidth;
	}

	float Word::height() const {
		return this->measuredHeight;
	}

	float Word::baseline() const {
		return this->measuredBaseline;
	}
} // namespace music_lyric_player::rendering::components::line::normal::main::syllable
