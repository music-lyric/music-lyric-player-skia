#include "backend/font/platform.h"
#include "include/core/SkFontMgr.h"
#include "include/ports/SkFontMgr_data.h"

namespace music_lyric_player::backend::font {
	sk_sp<SkFontMgr> createSystemFontManager() {
		// A browser hands WebAssembly no font stack to enumerate, so families can only come from what the page registers.
		return nullptr;
	}

	sk_sp<SkFontMgr> createDataFontManager(SkSpan<sk_sp<SkData>> files) {
		return SkFontMgr_New_Custom_Data(files);
	}
} // namespace music_lyric_player::backend::font
