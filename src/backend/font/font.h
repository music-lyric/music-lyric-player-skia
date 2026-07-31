#ifndef MUSIC_LYRIC_PLAYER_BACKEND_FONT_FONT_H_
#define MUSIC_LYRIC_PLAYER_BACKEND_FONT_FONT_H_

#include "include/core/SkRefCnt.h"

class SkData;
class SkFontMgr;

namespace music_lyric_player::backend::font {
	/**
	 * Adds one font file to the process-wide set that every later `createFontMgr()` lays over the platform's own font stack.
	 * Returns false and changes nothing when the data is empty or that exact file is already registered.
	 * Every registered file lands in the same layer, so one family's styles still group together when they are registered one call at a time.
	 */
	bool registerFontData(sk_sp<SkData> data);

	/**
	 * Creates the font manager every family lookup goes through: the registered files laid over the platform's own font stack.
	 * A family the registered files carry shadows the system one of that name, while every other name still resolves through the system.
	 * A platform with no font stack of its own resolves from the registered files alone, and a process with nothing registered resolves from the system alone.
	 */
	sk_sp<SkFontMgr> createFontMgr();
} // namespace music_lyric_player::backend::font

#endif // MUSIC_LYRIC_PLAYER_BACKEND_FONT_FONT_H_
