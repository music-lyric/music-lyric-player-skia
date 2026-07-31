#ifndef MUSIC_LYRIC_PLAYER_BACKEND_FONT_FONT_H_
#define MUSIC_LYRIC_PLAYER_BACKEND_FONT_FONT_H_

#include "include/core/SkRefCnt.h"

class SkData;
class SkFontMgr;

namespace music_lyric_player::backend::font {
	/**
	 * Adds one font file to the process-wide set that every later `fontManager()` lays over the platform's own font stack.
	 * Returns false and changes nothing when the data is empty or that exact file is already registered.
	 * Every registered file lands in the same layer, so one family's styles still group together when they are registered one call at a time.
	 * Handing the rebuilt manager to a renderer is left to the caller, which is what leaves this callable from whichever thread the host registers on.
	 */
	bool registerFontData(sk_sp<SkData> data);

	/**
	 * Returns the font manager every family lookup goes through: the registered files laid over the platform's own font stack.
	 * A family the registered files carry shadows the system one of that name, while every other name still resolves through the system.
	 * One manager is shared by the whole process, so renderers drawing the same font share its typefaces and the glyph caches Skia keys off them.
	 * A registration replaces it, which is why a caller holding one has to ask again for the new file to reach its lookups.
	 */
	sk_sp<SkFontMgr> fontManager();
} // namespace music_lyric_player::backend::font

#endif // MUSIC_LYRIC_PLAYER_BACKEND_FONT_FONT_H_
