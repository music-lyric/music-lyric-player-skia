#include "backend/font/font.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkSpan.h"
#include "include/ports/SkFontMgr_data.h"

namespace music_lyric_player::backend::font {
	sk_sp<SkFontMgr> createFontMgr() {
		// The browser exposes no system font stack, so families can only come from data registered at runtime.
		return SkFontMgr_New_Custom_Data({});
	}
} // namespace music_lyric_player::backend::font
