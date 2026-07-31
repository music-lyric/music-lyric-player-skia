#ifndef MUSIC_LYRIC_PLAYER_BACKEND_FONT_PLATFORM_H_
#define MUSIC_LYRIC_PLAYER_BACKEND_FONT_PLATFORM_H_

#include "include/core/SkRefCnt.h"
#include "include/core/SkSpan.h"

class SkData;
class SkFontMgr;

namespace music_lyric_player::backend::font {
	/**
	 * Opens the font stack the platform installs, or null where there is none to open.
	 * The web has no such stack to reach at all.
	 * Anywhere else a null means the platform's own font service refused, which leaves the registered files as the only source of families.
	 */
	sk_sp<SkFontMgr> createSystemFontManager();

	/**
	 * Builds a font manager resolving families out of `files`, or null where this build has no font reader to do it with.
	 * Reading a font file is Skia's FreeType work, and a platform whose Skia is built against the system font service alone does not carry it.
	 */
	sk_sp<SkFontMgr> createDataFontManager(SkSpan<sk_sp<SkData>> files);
} // namespace music_lyric_player::backend::font

#endif // MUSIC_LYRIC_PLAYER_BACKEND_FONT_PLATFORM_H_
