#include "render/components/line/normal/index.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "include/core/SkBlendMode.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkColorFilter.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkPaint.h"
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
#include "render/common/context.h"
#include "render/utils/length.h"

namespace tl = ::skia::textlayout;

namespace music_lyric_player::render::components::line::normal {
	namespace {
		// Numeric fallback mirroring the string default in the config schema, used when the value fails to parse.
		constexpr double kDefaultFontSize = 34.0; // line.font.size ("34px")

		/**
		 * Maps the config's integer alignment onto SkParagraph's `TextAlign`.
		 */
		tl::TextAlign toTextAlign(int align) {
			switch (align) {
			case 1:
				return tl::TextAlign::kCenter;
			case 2:
				return tl::TextAlign::kRight;
			default:
				return tl::TextAlign::kLeft;
			}
		}
	} // namespace

	Element::Element(int index, std::string text)
	    : base::Element(index),
	      text(std::move(text)) {}

	Element::~Element() = default;

	void Element::layout(float width, const common::RenderContext& context) {
		this->width = std::max(width, 1.0f);

		if (!context.fonts || !context.unicode) {
			this->paragraph      = nullptr;
			this->measuredHeight = 0.0f;
			return;
		}

		const config::Root& cfg = context.config;

		tl::TextStyle textStyle;
		textStyle.setColor(SK_ColorWHITE); // tinted per state at paint time via kModulate
		textStyle.setFontSize(static_cast<SkScalar>(resolveLength(cfg.line.font.size, kDefaultFontSize)));
		if (!cfg.line.font.family.empty()) {
			textStyle.setFontFamilies({SkString(cfg.line.font.family.c_str())});
		}
		textStyle.setFontStyle(SkFontStyle::Normal());

		tl::ParagraphStyle paraStyle;
		paraStyle.setTextStyle(textStyle);
		paraStyle.setTextAlign(toTextAlign(cfg.layout.align));

		std::unique_ptr<tl::ParagraphBuilder> builder = tl::ParagraphBuilder::make(paraStyle, context.fonts, context.unicode);
		if (!builder) {
			this->paragraph      = nullptr;
			this->measuredHeight = 0.0f;
			return;
		}
		builder->addText(this->text.c_str());
		this->paragraph = builder->Build();
		if (this->paragraph) {
			this->paragraph->layout(this->width);
			this->measuredHeight = this->paragraph->getHeight();
		} else {
			this->measuredHeight = 0.0f;
		}
	}

	void Element::paint(SkCanvas* canvas, float x, float y, bool active, const common::RenderContext& context) const {
		if (!this->paragraph) {
			return;
		}
		const config::Root& cfg   = context.config;
		const SkColor       color = static_cast<SkColor>(active ? cfg.line.active.color : cfg.line.normal.color);

		// The paragraph is opaque white; a modulate layer tints it to the state colour without re-shaping.
		SkPaint layerPaint;
		layerPaint.setColorFilter(SkColorFilters::Blend(color, SkBlendMode::kModulate));
		const SkRect bounds = SkRect::MakeXYWH(x, y, this->width, this->measuredHeight);
		canvas->saveLayer(&bounds, &layerPaint);
		this->paragraph->paint(canvas, x, y);
		canvas->restore();
	}
} // namespace music_lyric_player::render::components::line::normal
