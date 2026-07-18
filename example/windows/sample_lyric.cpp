#include "sample_lyric.h"

#include <cstdint>
#include <vector>

#include "sample_lyric_data.gen.h"

namespace example {
	music_lyric_model::parsed::Info buildSampleLyric() {
		return music_lyric_model::parsed::decodeParsedInfo(
		    std::vector<std::uint8_t>(kSampleLyricData.begin(), kSampleLyricData.end()));
	}
} // namespace example
