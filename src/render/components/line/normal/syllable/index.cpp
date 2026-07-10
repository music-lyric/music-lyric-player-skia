#include "render/components/line/normal/syllable/index.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "music_lyric_model.h"
#include "render/common/context.h"
#include "render/components/line/normal/syllable/word.h"
#include "render/utils/length.h"

namespace music_lyric_player::render::components::line::normal::syllable {
	namespace {
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

	Element::Element(const ::lyric::runtime::Line& info) {
		const auto& source = ::music_lyric_model::runtime::getLineWords(info);
		std::size_t count  = 0;
		for (const ::lyric::runtime::Word& item : source) {
			if (::music_lyric_model::runtime::isWordNormal(item)) {
				++count;
			}
		}

		this->words.reserve(count);
		bool        hasSpace = false;
		std::size_t index    = 0;
		for (const ::lyric::runtime::Word& item : source) {
			if (::music_lyric_model::runtime::isWordSpace(item)) {
				hasSpace = true;
				continue;
			}
			const ::lyric::runtime::WordNormal* normal = ::music_lyric_model::runtime::asWordNormal(item);
			if (normal == nullptr) {
				continue;
			}
			this->words.push_back(std::make_unique<Word>(*normal, index, count, hasSpace));
			hasSpace = false;
			++index;
		}
	}

	Element::~Element() = default;

	void Element::layout(float width, const common::RenderContext& context) {
		this->width          = std::max(width, 1.0f);
		this->measuredHeight = 0.0f;
		if (this->words.empty()) {
			return;
		}

		for (const std::unique_ptr<Word>& item : this->words) {
			item->layout(this->width, context);
		}

		const float      fontSize   = static_cast<float>(std::max(resolveLength(context.config.line.font.size, config::Default.line.font.size), 0.0));
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

	void Element::paint(SkCanvas* canvas, float x, float y, double now, bool active, const common::RenderContext& context) const {
		for (const std::unique_ptr<Word>& item : this->words) {
			item->paint(canvas, x, y, now, active, context);
		}
	}

	float Element::height() const {
		return this->measuredHeight;
	}
} // namespace music_lyric_player::render::components::line::normal::syllable
