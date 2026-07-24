#include "rendering/components/line/normal/main/plain/index.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMetrics.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkFontTypes.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPoint.h"
#include "include/core/SkScalar.h"
#include "include/core/SkTextBlob.h"
#include "include/core/SkTypeface.h"
#include "modules/skshaper/include/SkShaper.h"
#include "music_lyric_model.h"
#include "rendering/common/context.h"
#include "rendering/utils/length.h"
#include "rendering/utils/shaping/font.h"
#include "rendering/utils/shaping/glyph.h"
#include "rendering/utils/shaping/shaper.h"

namespace music_lyric_player::rendering::components::line::normal::main::plain {
	namespace {
		using Align = config::layout::Align;

		/**
		 * Computes the horizontal alignment offset that shifts a shaped line of the given width inside the block.
		 */
		float alignOffset(Align align, float blockWidth, float lineWidth) {
			switch (align) {
			case Align::Center:
				return (blockWidth - lineWidth) * 0.5f;
			case Align::Right:
				return blockWidth - lineWidth;
			case Align::Left:
			default:
				return 0.0f;
			}
		}
	} // namespace

	Element::Element(const music_lyric_model::parsed::Line& info)
	    : text(music_lyric_model::parsed::getParsedLineText(info)) {}

	Element::~Element() = default;

	void Element::layout(float width, const common::RenderContext& context) {
		this->width          = std::max(width, 1.0f);
		this->measuredHeight = 0.0f;
		this->lines.clear();

		if (!context.shaper || !context.fontMgr || !context.unicode || this->text.empty()) {
			return;
		}

		const config::Root& cfg  = context.config;
		const float         size = static_cast<float>(std::max(resolveLength(cfg.line.normal.main.syllable.font.size, config::Default.line.normal.main.syllable.font.size), 1.0));
		const SkFont        font = utils::shaping::buildBodyFont(context.fontMgr, cfg.line.normal.main.syllable.font.family.value(), size);

		const char*       utf8  = this->text.c_str();
		const std::size_t bytes = this->text.size();

		const utils::shaping::ShapedText shaped = utils::shaping::shapeText(*context.shaper, context.unicode, context.fontMgr, font, utf8, bytes, this->width);

		// Each wrapped line becomes its own blob; the captured positions already carry the block-absolute baseline, so only the horizontal alignment offset is resolved here.
		const Align align = cfg.layout.align;
		this->lines.reserve(shaped.lines.size());
		for (const utils::shaping::ShapedLine& line : shaped.lines) {
			SkTextBlobBuilder builder;
			utils::shaping::appendLine(builder, line, utf8);
			this->lines.push_back({builder.make(), alignOffset(align, this->width, line.width)});
			this->measuredHeight += line.ascent + line.descent;
		}
	}

	void Element::paint(SkCanvas* canvas, float x, float y, SkColor color) const {
		if (this->lines.empty()) {
			return;
		}

		SkPaint paint;
		paint.setAntiAlias(true);
		paint.setColor(color);
		for (const PaintLine& line : this->lines) {
			if (line.blob) {
				canvas->drawTextBlob(line.blob, x + line.offsetX, y, paint);
			}
		}
	}

	float Element::height() const {
		return this->measuredHeight;
	}
} // namespace music_lyric_player::rendering::components::line::normal::main::plain
