#include "rendering/components/line/normal/main/syllable/index.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "music_lyric_model.h"
#include "rendering/common/context.h"
#include "rendering/components/line/normal/main/syllable/word.h"
#include "rendering/utils/animation/easing.h"
#include "rendering/utils/layout/inline_flow.h"
#include "rendering/utils/length.h"

namespace music_lyric_player::rendering::components::line::normal::main::syllable {
	namespace {
		// The word's own opacity fade when a line deactivates; mirrors the web `.word` `transition: opacity 0.8s ease`.
		constexpr double kWordFadeDurationMs = 800.0;

		/**
		 * Returns the line's absolute start time, or zero when timing is absent.
		 */
		double lineStart(const music_lyric_model::parsed::Line& info) {
			const music_lyric_model::common::Time* time = music_lyric_model::parsed::getParsedLineTime(info);
			return time ? static_cast<double>(time->start) : 0.0;
		}

		/**
		 * Returns the line's non-negative duration.
		 */
		double lineDuration(const music_lyric_model::parsed::Line& info) {
			return std::max(static_cast<double>(music_lyric_model::parsed::getParsedLineDuration(info)), 0.0);
		}
	} // namespace

	Element::Element(const music_lyric_model::parsed::Line& info) : mask(lineStart(info), lineDuration(info)) {
		// CSS `ease` over 0.8s; only deactivation animates.
		this->wordOpacity.setEasing(::music_lyric_player::rendering::animation::CubicBezier{0.25f, 0.1f, 0.25f, 1.0f});
		this->wordOpacity.setDuration(kWordFadeDurationMs);

		const auto& source = music_lyric_model::parsed::getParsedLineWords(info);
		this->words.reserve(source.size());
		uint32_t spaces = 0;
		for (const music_lyric_model::common::Word& item : source) {
			if (music_lyric_model::common::isWordSpace(item)) {
				const music_lyric_model::common::WordSpace* space = music_lyric_model::common::asWordSpace(item);
				spaces += space ? space->count : 0;
				continue;
			}
			const music_lyric_model::common::WordNormal* normal = music_lyric_model::common::asWordNormal(item);
			if (normal == nullptr) {
				continue;
			}
			this->words.push_back(std::make_unique<Word>(*normal, spaces));
			spaces = 0;
		}
	}

	Element::~Element() = default;

	void Element::layout(float width, const common::RenderContext& context) {
		this->width          = std::max(width, 1.0f);
		this->measuredHeight = 0.0f;
		if (this->words.empty()) {
			return;
		}

		std::vector<animation::Mask::Input> maskInputs;
		maskInputs.reserve(this->words.size());
		for (const std::unique_ptr<Word>& item : this->words) {
			item->layout(context);
			maskInputs.push_back(item->maskInput());
		}
		this->mask.update(maskInputs);

		const float fontSize =
			static_cast<float>(std::max(resolveLength(context.config.line.normal.main.syllable.font.size, config::Default.line.normal.main.syllable.font.size), 0.0));
		const float spaceWidth = fontSize * 0.3f;

		std::vector<utils::layout::InlineCell> cells;
		cells.reserve(this->words.size());
		for (const std::unique_ptr<Word>& item : this->words) {
			utils::layout::InlineCell cell;
			cell.advance = item->width();
			cell.gap     = static_cast<float>(item->spacesBefore()) * spaceWidth;
			cell.ascent  = item->baseline();
			cell.descent = item->height() - item->baseline();
			cells.push_back(cell);
		}

		const utils::layout::InlineFlowLayout flow = utils::layout::layoutInlineFlow(cells, this->width, context.config.layout.align.value());
		for (std::size_t i = 0; i < this->words.size(); ++i) {
			this->words[i]->setPosition(flow.placements[i].x, flow.placements[i].y);
		}
		this->measuredHeight = flow.height;
	}

	void Element::paint(SkCanvas* canvas, float x, float y, double now, bool active, bool played, const common::RenderContext& context) const {
		const auto& style         = context.config.line.normal.main.syllable.style;
		const float normalOpacity = static_cast<float>(style.normal.opacity);
		const float activeOpacity = static_cast<float>(style.active.opacity);
		const float playedOpacity = static_cast<float>(style.played.opacity);

		// Drive the line-wide word opacity across state flips: activation snaps to the active brightness, every other change eases to the normal or played brightness.
		const float goal = active ? activeOpacity : (played ? playedOpacity : normalOpacity);
		if (!this->wordFadeReady) {
			this->wordOpacity.snap(goal);
			this->wordFadeReady = true;
		} else if (active && !this->wordWasActive) {
			this->wordOpacity.snap(goal);
		} else if (goal != this->wordOpacityGoal) {
			this->wordOpacity.retarget(now, goal);
		} else {
			// Stable state: track config edits without restarting an in-flight fade.
			this->wordOpacity.setTarget(goal);
		}
		this->wordWasActive         = active;
		this->wordOpacityGoal       = goal;
		const float inactiveOpacity = this->wordOpacity.sample(now);

		const auto& maskConfig = context.config.line.normal.main.syllable.word.animation.mask;
		if (active && maskConfig.enabled) {
			this->mask.sample(context.currentTime, maskConfig.feather.normal, maskConfig.feather.first, maskConfig.feather.last);
		}

		for (std::size_t i = 0; i < this->words.size(); ++i) {
			this->words[i]->paint(canvas, x, y, now, active, maskConfig.enabled, this->mask.progress(i), this->mask.feather(i), inactiveOpacity, context);
		}
	}

	float Element::height() const {
		return this->measuredHeight;
	}
} // namespace music_lyric_player::rendering::components::line::normal::main::syllable
