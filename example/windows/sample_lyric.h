#ifndef MUSIC_LYRIC_PLAYER_EXAMPLE_SAMPLE_LYRIC_H_
#define MUSIC_LYRIC_PLAYER_EXAMPLE_SAMPLE_LYRIC_H_

#include "runtime/info.pb.h"

namespace example {
	/**
	 * Decodes the embedded v3 protobuf lyric used by the demo.
	 */
	::lyric::runtime::Info buildSampleLyric();
} // namespace example

#endif
