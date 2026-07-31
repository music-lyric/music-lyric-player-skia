#include "backend/font/platform.h"
#include "include/core/SkFontMgr.h"
#include "include/ports/SkFontMgr_android.h"
#include "include/ports/SkFontMgr_android_ndk.h"
#include "include/ports/SkFontMgr_data.h"
#include "include/ports/SkFontScanner_FreeType.h"
#include "utils/logger/logger.h"

namespace music_lyric_player::backend::font {
	namespace {
		constexpr utils::Logger logger{"AndroidFont"};
	} // namespace

	sk_sp<SkFontMgr> createSystemFontManager() {
		// Files are left uncached so a family is mapped the first time something asks for it, rather than the whole system font set being read before the first frame.
		sk_sp<SkFontMgr> system = SkFontMgr_New_AndroidNDK(false, SkFontScanner_Make_FreeType());
		if (system != nullptr) {
			return system;
		}

		// The NDK font API arrived in Android 10, so anything older is read out of the configuration file the platform still ships.
		logger.info("the NDK font API is unavailable, reading the system font configuration instead");
		system = SkFontMgr_New_Android(nullptr, SkFontScanner_Make_FreeType());
		if (system == nullptr) {
			logger.error("no system font stack could be opened");
		}
		return system;
	}

	sk_sp<SkFontMgr> createDataFontManager(SkSpan<sk_sp<SkData>> files) {
		return SkFontMgr_New_Custom_Data(files);
	}
} // namespace music_lyric_player::backend::font
