#include "base/merger.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

#include "line/content.h"

namespace music_lyric_player::base {
	void Merger::build(const ::lyric::Info& info, double mergeWindow, int mergeLimit) {
		info_ = &info;

		const int count = info.lines_size();
		mergedEnd_.assign(static_cast<std::size_t>(count), 0.0);
		if (count == 0) {
			return;
		}

		const double threshold = std::max(0.0, mergeWindow);
		// A cap of 0 or less leaves the batch size unbounded.
		const double limit = mergeLimit > 0 ? static_cast<double>(mergeLimit) : std::numeric_limits<double>::infinity();

		double nextRaw                                  = getRawTime(count - 1);
		mergedEnd_[static_cast<std::size_t>(count - 1)] = nextRaw;
		double batchSize                                = 1.0;
		for (int i = count - 2; i >= 0; --i) {
			const double raw = getRawTime(i);
			// Within threshold and the batch still has room: extend to the cluster's later end so they leave together.
			if (threshold > 0.0 && batchSize < limit && std::abs(nextRaw - raw) < threshold) {
				mergedEnd_[static_cast<std::size_t>(i)] = std::max(raw, mergedEnd_[static_cast<std::size_t>(i + 1)]);
				batchSize += 1.0;
			} else {
				mergedEnd_[static_cast<std::size_t>(i)] = raw;
				batchSize                               = 1.0;
			}
			nextRaw = raw;
		}
	}

	double Merger::getMergedTime(int index) const {
		if (index < 0 || index >= static_cast<int>(mergedEnd_.size())) {
			return getRawTime(index);
		}
		return mergedEnd_[static_cast<std::size_t>(index)];
	}

	double Merger::getRawTime(int index) const {
		if (info_ == nullptr || index < 0 || index >= info_->lines_size()) {
			return 0.0;
		}
		if (index == info_->lines_size() - 1) {
			return std::numeric_limits<double>::infinity();
		}
		const ::lyric::Time* end   = music_lyric_model::getLineTime(info_->lines(index));
		const ::lyric::Time* start = music_lyric_model::getLineTime(info_->lines(index + 1));
		return std::max(end ? static_cast<double>(end->end()) : 0.0, start ? static_cast<double>(start->start()) : 0.0);
	}
} // namespace music_lyric_player::base
