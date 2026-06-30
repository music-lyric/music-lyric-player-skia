#include "base/offset.h"

#include <cmath>

#include "meta/meta.h"

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
		const ::lyric::MetaItem* meta = music_lyric_model::getFirstMeta(info.metas(), ::lyric::MetaItem::kOffset);
		meta_                         = meta && std::isfinite(meta->offset()) ? meta->offset() : 0.0;
	}

	double Offset::resolve(double global) const {
		const double value = global + meta_ + temp_;
		return std::isfinite(value) ? value : 0.0;
	}
} // namespace music_lyric_player::base
