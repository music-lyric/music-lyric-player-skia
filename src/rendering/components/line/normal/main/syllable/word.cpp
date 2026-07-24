#include "rendering/components/line/normal/main/syllable/word.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMetrics.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkFontTypes.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPoint.h"
#include "include/core/SkRect.h"
#include "include/core/SkScalar.h"
#include "include/core/SkTextBlob.h"
#include "include/core/SkTypeface.h"
#include "modules/skshaper/include/SkShaper.h"
#include "music_lyric_model.h"
#include "rendering/common/context.h"
#include "rendering/utils/color/parse.h"
#include "rendering/utils/length.h"
#include "rendering/utils/shaping/font.h"
#include "rendering/utils/shaping/glyph.h"
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
		this->blob             = nullptr;

		if (!context.shaper || !context.fontMgr || this->text.empty()) {
			return;
		}

		const config::Root& cfg  = context.config;
		const float         size = static_cast<float>(std::max(resolveLength(cfg.line.normal.main.syllable.font.size, config::Default.line.normal.main.syllable.font.size), 1.0));
		const SkFont        font = utils::shaping::buildBodyFont(context.fontMgr, cfg.line.normal.main.syllable.font.family.value(), size);

		const char*       utf8  = this->text.c_str();
		const std::size_t bytes = this->text.size();

		const utils::shaping::ShapedText shaped = utils::shaping::shapeText(*context.shaper, context.unicode, context.fontMgr, font, utf8, bytes, kUnboundedWidth);
		if (shaped.lines.empty()) {
			return;
		}

		// The word is shaped unbounded, so it is one line; the blob and metrics are rebuilt verbatim from the captured glyphs.
		SkTextBlobBuilder builder;
		float             maxWidth = 0.0f;
		for (const utils::shaping::ShapedLine& line : shaped.lines) {
			utils::shaping::appendLine(builder, line, utf8);
			maxWidth = std::max(maxWidth, line.width);
		}

		const utils::shaping::ShapedLine& last = shaped.lines.back();
		this->blob             = builder.make();
		this->measuredWidth    = std::ceil(std::max(maxWidth, 1.0f) + kWidthEpsilon);
		this->measuredBaseline = last.ascent;
		this->measuredHeight   = last.ascent + last.descent;
	}

	void Word::setPosition(float x, float y) {
		this->x = x;
		this->y = y;
	}

	void Word::paintBlob(SkCanvas* canvas, float x, float y, SkColor color) const {
		if (!this->blob) {
			return;
		}
		SkPaint paint;
		paint.setAntiAlias(true);
		paint.setColor(color);
		canvas->drawTextBlob(this->blob, x, y, paint);
	}

	void Word::paintReveal(SkCanvas* canvas, float x, float y, float progress, float feather, SkColor unsungColor, SkColor sungColor) const {
		if (!this->blob) {
			return;
		}

		const SkRect textBounds = SkRect::MakeXYWH(x, y, this->measuredWidth, this->measuredHeight);
		const SkRect drawBounds = textBounds.makeOutset(kGlyphOutset, kGlyphOutset);
		canvas->saveLayer(&drawBounds, nullptr);
		// Opaque coverage: the mask supplies the alpha, so the base glyph layer stays fully opaque here.
		this->paintBlob(canvas, x, y, SK_ColorWHITE);
		animation::Mask::apply(canvas, drawBounds, textBounds, progress, feather, unsungColor, sungColor);
		canvas->restore();
	}

	void Word::paint(SkCanvas* canvas, float lineX, float lineY, double now, bool active, bool maskEnabled, float maskProgress, float maskFeather, float inactiveOpacity, const common::RenderContext& context) const {
		if (!this->blob) {
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
			this->paintBlob(canvas, drawX, drawY, utils::color::withOpacity(normalColor, inactiveOpacity));
			return;
		}

		// The active line switches the rgb to the active color at once; the mask only wipes alpha, from the normal opacity (unsung) up to the active opacity (sung), so the hue never changes.
		const SkColor sungColor   = utils::color::withOpacity(activeColor, activeOpacity);
		const SkColor unsungColor = utils::color::withOpacity(activeColor, normalOpacity);

		if (!maskEnabled || maskProgress >= 1.0f) {
			this->paintBlob(canvas, drawX, drawY, sungColor);
			return;
		}
		if (maskProgress <= 0.0f) {
			this->paintBlob(canvas, drawX, drawY, unsungColor);
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
