#include "backend/font/font.h"
#include "backend/font/registry.h"
#include "include/core/SkFontMgr.h"
#include "include/ports/SkFontMgr_data.h"

namespace music_lyric_player::backend::font {
	sk_sp<SkFontMgr> createFontMgr() {
		// The browser exposes no system font stack, so families can only come from data registered at runtime.
		// The result is immutable, which is why a later registration builds a new manager instead of mutating this one.
		return SkFontMgr_New_Custom_Data(registeredFonts());
	}
} // namespace music_lyric_player::backend::font
