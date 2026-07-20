#include "rendering/components/container/index.h"

#include <algorithm>

#include "include/core/SkCanvas.h"
#include "rendering/common/context.h"
#include "rendering/utils/color/parse.h"
#include "rendering/utils/length.h"

namespace music_lyric_player::rendering::components::container {
	void Element::paintBackground(SkCanvas* canvas, const common::RenderContext& context) const {
		const config::Root& cfg = context.config;
		// clear ignores the canvas transform, so the fill always covers the whole surface.
		canvas->clear(utils::color::resolve(cfg.container.backgroundColor, config::Default.container.backgroundColor));
	}

	float Element::paddingX(float logicalWidth, const common::RenderContext& context) const {
		const config::Root& cfg     = context.config;
		const float         padding = static_cast<float>(resolveLength(cfg.container.paddingX, config::Default.container.paddingX, logicalWidth));
		// Clamp so both sides together never exceed the viewport, keeping the content width positive.
		return std::min(padding, logicalWidth * 0.5f);
	}
} // namespace music_lyric_player::rendering::components::container
