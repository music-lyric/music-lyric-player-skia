#include "render/core/layout.h"

namespace music_lyric_player::render::core {
	void LayoutManager::update(const std::vector<std::unique_ptr<components::line::base::Element>>& lines, float gap) {
		tops_.resize(lines.size());
		float cursor = 0.0f;
		for (std::size_t i = 0; i < lines.size(); ++i) {
			tops_[i] = cursor;
			cursor += lines[i]->height() + gap;
		}
	}
} // namespace music_lyric_player::render::core
