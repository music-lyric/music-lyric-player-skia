#include "render/components/line/normal/plain/index.h"

#include <algorithm>
#include <memory>

#include "include/core/SkBlendMode.h"
#include "include/core/SkCanvas.h"
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
#include "music_lyric_model.h"
#include "render/common/context.h"
#include "render/utils/length.h"

namespace tl = ::skia::textlayout;

namespace music_lyric_player::render::components::line::normal::plain {
	namespace {
		using Align = config::layout::Align;

		/**
		 * Maps the configured line alignment onto SkParagraph's text alignment.
		 */
		tl::TextAlign toTextAlign(Align align) {
			switch (align) {
			case Align::Center:
				return tl::TextAlign::kCenter;
			case Align::Right:
				return tl::TextAlign::kRight;
			case Align::Left:
			default:
				return tl::TextAlign::kLeft;
			}
		}
	} // namespace

	Element::Element(const ::lyric::runtime::Line& info)
	    : text(::music_lyric_model::runtime::getLineText(info)) {}

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
		textStyle.setColor(SK_ColorWHITE);
		textStyle.setFontSize(static_cast<SkScalar>(resolveLength(cfg.line.font.size, config::Default.line.font.size)));
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

	void Element::paint(SkCanvas* canvas, float x, float y, SkColor color) const {
		if (!this->paragraph) {
			return;
		}

		SkPaint layerPaint;
		layerPaint.setColorFilter(SkColorFilters::Blend(color, SkBlendMode::kModulate));
		const SkRect bounds = SkRect::MakeXYWH(x, y, this->width, this->measuredHeight);
		canvas->saveLayer(&bounds, &layerPaint);
		this->paragraph->paint(canvas, x, y);
		canvas->restore();
	}

	float Element::height() const {
		return this->measuredHeight;
	}
} // namespace music_lyric_player::render::components::line::normal::plain
