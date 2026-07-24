#include "rendering/components/line/normal/main/plain/index.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkFont.h"
#include "include/core/SkRect.h"
#include "music_lyric_model.h"
#include "rendering/common/context.h"
#include "rendering/utils/fragment/builder.h"
#include "rendering/utils/fragment/glyph.h"
#include "rendering/utils/length.h"
#include "rendering/utils/shaping/font.h"
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
		this->group          = {};

		if (!context.shaper || !context.fontMgr || !context.unicode || this->text.empty()) {
			return;
		}

		const config::Root& cfg  = context.config;
		const float         size = static_cast<float>(std::max(resolveLength(cfg.line.normal.main.syllable.font.size, config::Default.line.normal.main.syllable.font.size), 1.0));
		const SkFont        font = utils::shaping::buildBodyFont(context.fontMgr, cfg.line.normal.main.syllable.font.family.value(), size);

		const char*       utf8  = this->text.c_str();
		const std::size_t bytes = this->text.size();

		const utils::shaping::ShapedText shaped = utils::shaping::shapeText(*context.shaper, context.unicode, context.fontMgr, font, utf8, bytes, this->width);

		// One fragment per wrapped line; alignment offset is baked into origin.x so paint is a single group call.
		const Align align = cfg.layout.align;
		this->group.fragments.reserve(shaped.lines.size());
		float maxWidth = 0.0f;
		for (const utils::shaping::ShapedLine& line : shaped.lines) {
			utils::fragment::FragmentGroup lineGroup = utils::fragment::makeLineGroup(line, utf8);
			if (lineGroup.fragments.empty()) {
				continue;
			}
			utils::fragment::GlyphFragment fragment = std::move(lineGroup.fragments.front());
			fragment.origin.fX                      = alignOffset(align, this->width, line.width);
			this->group.fragments.push_back(std::move(fragment));
			maxWidth = std::max(maxWidth, line.width);
			this->measuredHeight += line.ascent + line.descent;
		}
		this->group.advance = maxWidth;
		this->group.height  = this->measuredHeight;
		this->group.bounds  = SkRect::MakeXYWH(0.0f, 0.0f, maxWidth, this->measuredHeight);
	}

	void Element::paint(SkCanvas* canvas, float x, float y, SkColor color) const {
		this->group.paint(canvas, x, y, color);
	}

	float Element::height() const {
		return this->measuredHeight;
	}
} // namespace music_lyric_player::rendering::components::line::normal::main::plain
