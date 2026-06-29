#include "base/offset.h"

#include <cmath>
#include <optional>

#include "lyric/access.h"

namespace music_lyric_player::base {
	void Offset::setTemp(double value) {
		temp_ = std::isfinite(value) ? value : 0.0;
	}

	void Offset::resetTemp() {
		temp_ = 0.0;
	}

	void Offset::refreshFromMeta(const ::lyric::Info& info, bool useMeta) {
		if (!useMeta) {
			meta_ = 0.0;
			return;
		}
		const std::optional<double> value = metaOffsetMs(info);
		meta_                             = value && std::isfinite(*value) ? *value : 0.0;
	}

	double Offset::resolve(double global) const {
		const double value = global + meta_ + temp_;
		return std::isfinite(value) ? value : 0.0;
	}
} // namespace music_lyric_player::base
