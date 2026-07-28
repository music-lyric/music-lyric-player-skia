#ifndef MUSIC_LYRIC_PLAYER_BACKEND_FONT_REGISTRY_H_
#define MUSIC_LYRIC_PLAYER_BACKEND_FONT_REGISTRY_H_

#include <cstddef>

#include "include/core/SkRefCnt.h"

class SkData;

namespace music_lyric_player::backend::font {
	/**
	 * Adds one font file to the process-wide set every later `createFontMgr()` call builds from.
	 * Returns false and leaves the generation untouched when the data is empty or already registered.
	 * Only the backends without a system font stack implement this, the web being the one that has none.
	 */
	bool registerFontData(sk_sp<SkData> data);

	/**
	 * Returns how many times the registered set has changed, so a caller can tell whether its font manager is stale.
	 */
	std::size_t fontGeneration();
} // namespace music_lyric_player::backend::font

#endif // MUSIC_LYRIC_PLAYER_BACKEND_FONT_REGISTRY_H_
