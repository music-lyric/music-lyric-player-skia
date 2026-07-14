#include "backend/font/font.h"

#include "include/core/SkFontMgr.h"
#include "include/ports/SkTypeface_win.h"

namespace music_lyric_player::backend::font {
	sk_sp<SkFontMgr> createFontMgr() {
		return SkFontMgr_New_DirectWrite();
	}
} // namespace music_lyric_player::backend::font
