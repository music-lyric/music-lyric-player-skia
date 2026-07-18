#include "rendering/core/line.h"

#include <memory>

#include "rendering/components/line/interlude/index.h"
#include "rendering/components/line/normal/index.h"

namespace music_lyric_player::rendering::core {
	void LineManager::rebuild(const music_lyric_model::parsed::Info& info) {
		this->lines.clear();
		this->lines.reserve(info.lines.size());
		const bool isSyllable = info.timing == music_lyric_model::common::Timing::Word;
		for (std::size_t i = 0; i < info.lines.size(); ++i) {
			const music_lyric_model::parsed::Line& line = info.lines[i];
			if (music_lyric_model::parsed::isParsedLineInterlude(line)) {
				this->lines.push_back(std::make_unique<components::line::interlude::Element>(static_cast<int>(i), line));
			} else {
				this->lines.push_back(std::make_unique<components::line::normal::Element>(static_cast<int>(i), line, isSyllable));
			}
		}
		this->layoutDirty = true;
	}

	void LineManager::clear() {
		this->lines.clear();
		this->layoutDirty = true;
	}

	void LineManager::invalidateLayout() {
		this->layoutDirty = true;
	}

	void LineManager::ensureLayout(float contentWidth, const common::RenderContext& context) {
		if (!this->layoutDirty && contentWidth == this->layoutWidth) {
			return;
		}
		for (const std::unique_ptr<components::line::base::Element>& line : this->lines) {
			line->layout(contentWidth, context);
		}
		this->layoutDirty = false;
		this->layoutWidth = contentWidth;
	}
} // namespace music_lyric_player::rendering::core
