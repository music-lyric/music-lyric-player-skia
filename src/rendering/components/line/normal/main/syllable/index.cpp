#include "rendering/components/line/normal/main/syllable/index.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "music_lyric_model.h"
#include "rendering/common/context.h"
#include "rendering/components/line/normal/main/syllable/word.h"
#include "rendering/utils/animation/easing.h"
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

		struct RowEntry {
			Word* word = nullptr;
			float x    = 0.0f;
		};

		struct Row {
			std::vector<RowEntry> entries;
			float                 width   = 0.0f;
			float                 ascent  = 0.0f;
			float                 descent = 0.0f;
		};
	} // namespace

	Element::Element(const music_lyric_model::parsed::Line& info)
	    : mask(lineStart(info), lineDuration(info)) {
		// CSS `ease` over 0.8s; only deactivation animates.
		this->wordOpacity.setEasing(::music_lyric_player::rendering::animation::CubicBezier{0.25f, 0.1f, 0.25f, 1.0f});
		this->wordOpacity.setDuration(kWordFadeDurationMs);

		const auto& source = music_lyric_model::parsed::getParsedLineWords(info);
		this->words.reserve(source.size());
		bool hasSpace = false;
		for (const music_lyric_model::common::Word& item : source) {
			if (music_lyric_model::common::isWordSpace(item)) {
				hasSpace = true;
				continue;
			}
			const music_lyric_model::common::WordNormal* normal = music_lyric_model::common::asWordNormal(item);
			if (normal == nullptr) {
				continue;
			}
			this->words.push_back(std::make_unique<Word>(*normal, hasSpace));
			hasSpace = false;
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

		const float      fontSize   = static_cast<float>(std::max(resolveLength(context.config.line.normal.main.syllable.font.size, config::Default.line.normal.main.syllable.font.size), 0.0));
		const float      spaceWidth = fontSize * 0.3f;
		std::vector<Row> rows;
		Row              row;
		for (const std::unique_ptr<Word>& item : this->words) {
			float gap = !row.entries.empty() && item->hasSpaceBefore() ? spaceWidth : 0.0f;
			if (!row.entries.empty() && row.width + gap + item->width() > this->width) {
				rows.push_back(std::move(row));
				row = Row{};
				gap = 0.0f;
			}

			row.entries.push_back(RowEntry{item.get(), row.width + gap});
			row.width += gap + item->width();
			row.ascent  = std::max(row.ascent, item->baseline());
			row.descent = std::max(row.descent, item->height() - item->baseline());
		}
		if (!row.entries.empty()) {
			rows.push_back(std::move(row));
		}

		float rowY = 0.0f;
		for (Row& current : rows) {
			float offsetX = 0.0f;
			switch (context.config.layout.align) {
			case config::layout::Align::Center:
				offsetX = (this->width - current.width) * 0.5f;
				break;
			case config::layout::Align::Right:
				offsetX = this->width - current.width;
				break;
			case config::layout::Align::Left:
			default:
				break;
			}

			for (const RowEntry& entry : current.entries) {
				entry.word->setPosition(offsetX + entry.x, rowY + current.ascent - entry.word->baseline());
			}
			rowY += current.ascent + current.descent;
		}
		this->measuredHeight = rowY;
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
