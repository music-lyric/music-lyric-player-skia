#include "rendering/components/line/normal/annotation/index.h"

#include <algorithm>
#include <utility>

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkFont.h"
#include "rendering/common/context.h"
#include "rendering/utils/animation/easing.h"
#include "rendering/utils/color/parse.h"
#include "rendering/utils/layout/paragraph.h"

namespace music_lyric_player::rendering::components::line::normal::annotation {
	namespace {
		// The row tint transition length; mirrors the shared line tint in `base::Element::stateColor` (a dedicated config is not ported yet).
		constexpr double kStateColorDuration = 300.0;

		/**
		 * Resolves one render state into its tinted color, then scales its alpha by the state opacity.
		 */
		SkColor stateStyle(const config::common::StyleConfig& style) {
			return utils::color::withOpacity(utils::color::resolve(style.color), style.opacity);
		}
	} // namespace

	Row::Row(std::string text) : text(std::move(text)) {
		this->colorTween.setEasing(animation::inOutCubic);
	}

	Row::~Row() = default;

	void Row::layout(float width, const common::RenderContext& context, const SkFont& font, config::layout::Align align) {
		this->measuredHeight = 0.0f;
		this->group          = {};

		if (!context.shaper || !context.fontMgr || !context.unicode || this->text.empty()) {
			return;
		}

		const float                    blockWidth = std::max(width, 1.0f);
		utils::layout::ParagraphLayout result =
			utils::layout::layoutParagraph(*context.shaper, context.unicode, context.fontMgr, font, this->text.c_str(), this->text.size(), blockWidth, align);

		this->group          = std::move(result.group);
		this->measuredHeight = result.height;
	}

	void Row::paint(SkCanvas* canvas, float x, float y, double now, bool active, bool played, const config::common::StateStyleConfig& style) const {
		if (this->group.fragments.empty()) {
			return;
		}

		const SkColor normalColor = stateStyle(style.normal);
		const SkColor activeColor = stateStyle(style.active);
		const SkColor playedColor = stateStyle(style.played);

		const SkColor target = active ? activeColor : (played ? playedColor : normalColor);
		const int     state  = active ? 2 : (played ? 1 : 0);
		if (!this->colorReady) {
			this->colorTween.snap(target);
			this->colorState = state;
			this->colorReady = true;
		} else if (state != this->colorState) {
			this->colorTween.setDuration(kStateColorDuration);
			this->colorTween.retarget(now, target);
			this->colorState = state;
		} else {
			// Stable state: track config color edits without restarting an in-flight fade.
			this->colorTween.setTarget(target);
		}

		this->group.paint(canvas, x, y, this->colorTween.sample(now));
	}

	float Row::height() const {
		return this->measuredHeight;
	}

	bool Row::empty() const {
		return this->text.empty() || this->group.fragments.empty();
	}
} // namespace music_lyric_player::rendering::components::line::normal::annotation
