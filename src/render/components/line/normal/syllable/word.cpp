#include "render/components/line/normal/syllable/word.h"

#include <algorithm>
#include <cmath>
#include <memory>

#include "include/core/SkCanvas.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkRect.h"
#include "include/core/SkScalar.h"
#include "include/core/SkString.h"
#include "modules/skparagraph/include/DartTypes.h"
#include "modules/skparagraph/include/FontCollection.h"
#include "modules/skparagraph/include/Paragraph.h"
#include "modules/skparagraph/include/ParagraphBuilder.h"
#include "modules/skparagraph/include/ParagraphStyle.h"
#include "modules/skparagraph/include/TextStyle.h"
#include "modules/skunicode/include/SkUnicode.h"
#include "music_lyric_model.h"
#include "render/common/context.h"
#include "render/utils/color/parse.h"
#include "render/utils/length.h"

namespace tl = ::skia::textlayout;

namespace music_lyric_player::render::components::line::normal::syllable {
	namespace {
		/**
		 * Returns the word's absolute start time, or zero when timing is absent.
		 */
		double wordStart(const ::lyric::runtime::WordNormal& info) {
			const ::lyric::common::Time* time = ::music_lyric_model::runtime::getWordTime(info);
			return time ? static_cast<double>(time->start()) : 0.0;
		}

		/**
		 * Returns the word's non-negative duration.
		 */
		double wordDuration(const ::lyric::runtime::WordNormal& info) {
			return std::max(static_cast<double>(::music_lyric_model::runtime::getWordDuration(info)), 0.0);
		}
	} // namespace

	Word::Word(const ::lyric::runtime::WordNormal& info, std::size_t index, std::size_t count, bool hasSpaceBefore)
	    : text(info.content()),
	      spaceBefore(hasSpaceBefore),
	      floating(wordStart(info), wordDuration(info)),
	      mask(wordStart(info), wordDuration(info), index, count) {}

	Word::~Word() = default;

	void Word::layout(float maxWidth, const common::RenderContext& context) {
		this->measuredWidth    = 0.0f;
		this->measuredHeight   = 0.0f;
		this->measuredBaseline = 0.0f;
		this->normalParagraph  = nullptr;
		this->activeParagraph  = nullptr;

		if (!context.fonts || !context.unicode || this->text.empty()) {
			return;
		}

		const config::Root& cfg         = context.config;
		const SkColor       normalColor = utils::color::resolve(cfg.line.style.normal.color, config::Default.line.style.normal.color);
		const SkColor       activeColor = utils::color::resolve(cfg.line.style.active.color, config::Default.line.style.active.color);
		const float         available   = std::max(maxWidth, 1.0f);

		this->normalParagraph = this->buildParagraph(available, context, normalColor);
		if (!this->normalParagraph) {
			return;
		}

		const float intrinsic = std::max(static_cast<float>(this->normalParagraph->getMaxIntrinsicWidth()), 1.0f);
		this->measuredWidth   = std::min(intrinsic, available);
		this->normalParagraph->layout(this->measuredWidth);
		this->activeParagraph  = this->buildParagraph(this->measuredWidth, context, activeColor);
		this->measuredHeight   = static_cast<float>(this->normalParagraph->getHeight());
		this->measuredBaseline = static_cast<float>(this->normalParagraph->getAlphabeticBaseline());
	}

	void Word::setPosition(float x, float y) {
		this->x = x;
		this->y = y;
	}

	std::unique_ptr<tl::Paragraph> Word::buildParagraph(float width, const common::RenderContext& context, SkColor color) const {
		const config::Root& cfg = context.config;

		tl::TextStyle textStyle;
		textStyle.setColor(color);
		textStyle.setFontSize(static_cast<SkScalar>(resolveLength(cfg.line.font.size, config::Default.line.font.size)));
		if (!cfg.line.font.family.empty()) {
			textStyle.setFontFamilies({SkString(cfg.line.font.family.c_str())});
		}
		textStyle.setFontStyle(SkFontStyle::Normal());

		tl::ParagraphStyle paragraphStyle;
		paragraphStyle.setTextStyle(textStyle);
		paragraphStyle.setTextAlign(tl::TextAlign::kLeft);

		std::unique_ptr<tl::ParagraphBuilder> builder = tl::ParagraphBuilder::make(paragraphStyle, context.fonts, context.unicode);
		if (!builder) {
			return nullptr;
		}
		builder->addText(this->text.c_str());
		std::unique_ptr<tl::Paragraph> paragraph = builder->Build();
		if (paragraph) {
			paragraph->layout(std::max(width, 1.0f));
		}
		return paragraph;
	}

	void Word::paintParagraph(SkCanvas* canvas, tl::Paragraph& paragraph, float x, float y) const {
		paragraph.paint(canvas, x, y);
	}

	void Word::paintReveal(SkCanvas* canvas, float x, float y, float progress, float feather) const {
		if (!this->activeParagraph || progress <= 0.0f) {
			return;
		}
		if (progress >= 1.0f || feather <= 0.0f) {
			if (progress >= 1.0f) {
				this->paintParagraph(canvas, *this->activeParagraph, x, y);
				return;
			}
			canvas->save();
			canvas->clipRect(SkRect::MakeXYWH(x, y, this->measuredWidth * progress, this->measuredHeight), true);
			this->paintParagraph(canvas, *this->activeParagraph, x, y);
			canvas->restore();
			return;
		}

		const SkRect bounds = SkRect::MakeXYWH(x, y, this->measuredWidth, this->measuredHeight);
		canvas->saveLayer(&bounds, nullptr);
		this->paintParagraph(canvas, *this->activeParagraph, x, y);

		this->mask.apply(canvas, bounds, progress, feather);
		canvas->restore();
	}

	void Word::paint(SkCanvas* canvas, float lineX, float lineY, double now, bool active, const common::RenderContext& context) const {
		if (!this->normalParagraph) {
			return;
		}

		const auto&  animationConfig = context.config.line.normal.main.syllable.word.animation;
		const double fromValue       = std::isfinite(animationConfig.floating.from)
			      ? animationConfig.floating.from
			      : config::Default.line.normal.main.syllable.word.animation.floating.from;
		const double toValue         = std::isfinite(animationConfig.floating.to)
				? animationConfig.floating.to
				: config::Default.line.normal.main.syllable.word.animation.floating.to;
		const float  offset          = this->floating.sample(now, context.currentTime, active, animationConfig.floating.enabled, static_cast<float>(fromValue), static_cast<float>(toValue));
		const float  drawX           = lineX + this->x;
		const float  drawY           = lineY + this->y + offset;

		this->paintParagraph(canvas, *this->normalParagraph, drawX, drawY);
		if (active) {
			if (!animationConfig.mask.enabled) {
				if (this->activeParagraph) {
					this->paintParagraph(canvas, *this->activeParagraph, drawX, drawY);
				}
			} else {
				const auto& featherConfig = animationConfig.mask.feather;
				const float feather       = this->mask.feather(this->measuredHeight, featherConfig.normal, featherConfig.first, featherConfig.last);
				this->paintReveal(canvas, drawX, drawY, this->mask.progress(context.currentTime), feather);
			}
		}
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
} // namespace music_lyric_player::render::components::line::normal::syllable
