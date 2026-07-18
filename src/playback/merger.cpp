#include "playback/merger.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace music_lyric_player::playback {
	void Merger::build(const music_lyric_model::parsed::Info& info, double mergeWindow, int mergeLimit) {
		this->info = &info;

		const int count = static_cast<int>(info.lines.size());
		this->mergedEnd.assign(static_cast<std::size_t>(count), 0.0);
		if (count == 0) {
			return;
		}

		const double threshold = std::max(0.0, mergeWindow);
		// A cap of 0 or less leaves the batch size unbounded.
		const double limit = mergeLimit > 0 ? static_cast<double>(mergeLimit) : std::numeric_limits<double>::infinity();

		double nextRaw                                       = getRawTime(count - 1);
		this->mergedEnd[static_cast<std::size_t>(count - 1)] = nextRaw;
		double batchSize                                     = 1.0;
		for (int i = count - 2; i >= 0; --i) {
			const double raw = getRawTime(i);
			// Within threshold and the batch still has room: extend to the cluster's later end so they leave together.
			if (threshold > 0.0 && batchSize < limit && std::abs(nextRaw - raw) < threshold) {
				this->mergedEnd[static_cast<std::size_t>(i)] = std::max(raw, this->mergedEnd[static_cast<std::size_t>(i + 1)]);
				batchSize += 1.0;
			} else {
				this->mergedEnd[static_cast<std::size_t>(i)] = raw;
				batchSize                                    = 1.0;
			}
			nextRaw = raw;
		}
	}

	double Merger::getMergedTime(int index) const {
		if (index < 0 || index >= static_cast<int>(this->mergedEnd.size())) {
			return getRawTime(index);
		}
		return this->mergedEnd[static_cast<std::size_t>(index)];
	}

	double Merger::getRawTime(int index) const {
		if (this->info == nullptr || index < 0 || index >= static_cast<int>(this->info->lines.size())) {
			return 0.0;
		}
		if (index == static_cast<int>(this->info->lines.size()) - 1) {
			return std::numeric_limits<double>::infinity();
		}
		const music_lyric_model::common::Time* end   = music_lyric_model::parsed::getParsedLineTime(this->info->lines[static_cast<std::size_t>(index)]);
		const music_lyric_model::common::Time* start = music_lyric_model::parsed::getParsedLineTime(this->info->lines[static_cast<std::size_t>(index + 1)]);
		return std::max(end ? static_cast<double>(end->end) : 0.0, start ? static_cast<double>(start->start) : 0.0);
	}
} // namespace music_lyric_player::playback
