#include "rendering/core/layout.h"

namespace music_lyric_player::rendering::core {
	void LayoutManager::update(const std::vector<std::unique_ptr<components::line::base::Element>>& lines, float gap) {
		this->tops.resize(lines.size());
		float cursor = 0.0f;
		for (std::size_t i = 0; i < lines.size(); ++i) {
			this->tops[i] = cursor;
			cursor += lines[i]->height() + gap;
		}
	}
} // namespace music_lyric_player::rendering::core
