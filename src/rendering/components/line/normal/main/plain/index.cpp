#include "rendering/components/line/normal/main/plain/index.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkFont.h"
#include "music_lyric_model.h"
#include "rendering/common/context.h"
#include "rendering/utils/layout/paragraph.h"
#include "rendering/utils/length.h"
#include "rendering/utils/shaping/font.h"

namespace music_lyric_player::rendering::components::line::normal::main::plain {
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

		utils::layout::ParagraphLayout result = utils::layout::layoutParagraph(*context.shaper, context.unicode, context.fontMgr, font, this->text.c_str(), this->text.size(), this->width, cfg.layout.align.value());

		this->group          = std::move(result.group);
		this->measuredHeight = result.height;
	}

	void Element::paint(SkCanvas* canvas, float x, float y, SkColor color) const {
		this->group.paint(canvas, x, y, color);
	}

	float Element::height() const {
		return this->measuredHeight;
	}
} // namespace music_lyric_player::rendering::components::line::normal::main::plain
