#ifndef MUSIC_LYRIC_PLAYER_EXAMPLE_SAMPLE_LYRIC_H_
#define MUSIC_LYRIC_PLAYER_EXAMPLE_SAMPLE_LYRIC_H_

#include "music_lyric_model.h"

namespace example {
	/**
	 * Decodes the embedded protobuf lyric used by the demo.
	 */
	music_lyric_model::parsed::Info buildSampleLyric();
} // namespace example

#endif
