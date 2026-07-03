#ifndef MUSIC_LYRIC_PLAYER_EXAMPLE_SAMPLE_LYRIC_H_
#define MUSIC_LYRIC_PLAYER_EXAMPLE_SAMPLE_LYRIC_H_

#include "info.pb.h"

namespace example {
	/**
	 * Builds a small hand-authored, line-timed lyric for the demo, including interludes, CJK text and
	 * a deliberately long line to exercise wrapping.
	 */
	::lyric::Info buildSampleLyric();
} // namespace example

#endif
