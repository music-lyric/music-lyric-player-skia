#include "backend/font/platform.h"
#include "include/core/SkFontMgr.h"
#include "include/ports/SkTypeface_win.h"
#include "utils/logger/logger.h"

namespace music_lyric_player::backend::font {
	namespace {
		constexpr utils::Logger logger{"WindowsFont"};
	} // namespace

	sk_sp<SkFontMgr> createSystemFontManager() {
		return SkFontMgr_New_DirectWrite();
	}

	sk_sp<SkFontMgr> createDataFontManager(SkSpan<sk_sp<SkData>> files) {
		// Skia is built here against DirectWrite alone, which leaves FreeType out, and reading a font file into a family is FreeType's work.
		// Lifting this means adding skia_use_freetype to the Windows GN arguments and rebuilding Skia, which is worth doing once a host on this platform can register a font at all.
		if (!files.empty()) {
			logger.warn("ignoring %zu registered font file(s): this build resolves families through DirectWrite alone", files.size());
		}
		return nullptr;
	}
} // namespace music_lyric_player::backend::font
