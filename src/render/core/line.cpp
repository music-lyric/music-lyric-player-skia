#include "render/core/line.h"

#include <algorithm>
#include <memory>

#include "music_lyric_model.h"
#include "render/components/line/interlude/index.h"
#include "render/components/line/normal/index.h"

namespace music_lyric_player::render::core {
	void LineManager::rebuild(const ::lyric::runtime::Info& info) {
		this->lines.clear();
		this->lines.reserve(static_cast<std::size_t>(std::max(info.lines_size(), 0)));
		for (int i = 0; i < info.lines_size(); ++i) {
			const ::lyric::runtime::Line& line = info.lines(i);
			if (::music_lyric_model::runtime::isLineInterlude(line)) {
				this->lines.push_back(std::make_unique<components::line::interlude::Element>(i, line));
			} else {
				this->lines.push_back(std::make_unique<components::line::normal::Element>(i, line));
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
} // namespace music_lyric_player::render::core
