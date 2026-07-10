#include "sample_lyric.h"

#include <cstdint>
#include <vector>

#include "music_lyric_model.h"
#include "sample_lyric_data.gen.h"

namespace example {
	::lyric::runtime::Info buildSampleLyric() {
		return ::music_lyric_model::runtime::decodeInfo(
		    std::vector<std::uint8_t>(kSampleLyricData.begin(), kSampleLyricData.end()));
	}
} // namespace example
