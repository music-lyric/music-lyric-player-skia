#include "playback/offset.h"

#include <cmath>

namespace music_lyric_player::playback {
	void Offset::setTemp(double value) {
		this->temp = std::isfinite(value) ? value : 0.0;
	}

	void Offset::resetTemp() {
		this->temp = 0.0;
	}

	void Offset::refreshFromMeta(const music_lyric_model::parsed::Info& info, bool useMeta) {
		if (!useMeta) {
			this->meta = 0.0;
			return;
		}
		// Meta carries a single lyric offset in milliseconds.
		this->meta = static_cast<double>(info.meta.offset);
	}

	double Offset::resolve(double global) const {
		const double value = global + this->meta + this->temp;
		return std::isfinite(value) ? value : 0.0;
	}
} // namespace music_lyric_player::playback
