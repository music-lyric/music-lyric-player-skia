#ifndef MUSIC_LYRIC_PLAYER_BACKEND_FONT_REGISTRY_H_
#define MUSIC_LYRIC_PLAYER_BACKEND_FONT_REGISTRY_H_

#include <cstddef>

#include "include/core/SkRefCnt.h"
#include "include/core/SkSpan.h"

class SkData;

namespace music_lyric_player::backend::font {
	/**
	 * Adds one font file to the process-wide set every later `createFontMgr()` call builds from.
	 * Returns false and leaves the generation untouched when the data is empty or already registered.
	 * A backend without a system font stack resolves families from this set alone, while one that has a stack layers the set over it.
	 */
	bool registerFontData(sk_sp<SkData> data);

	/**
	 * Returns how many times the registered set has changed, so a caller can tell whether its font manager is stale.
	 */
	std::size_t fontGeneration();

	/**
	 * Returns the registered files in registration order, for the `createFontMgr()` that has to build a manager over them.
	 * A later registration can move the underlying storage, so the span is only good until the next `registerFontData()`.
	 */
	SkSpan<sk_sp<SkData>> registeredFonts();
} // namespace music_lyric_player::backend::font

#endif // MUSIC_LYRIC_PLAYER_BACKEND_FONT_REGISTRY_H_
