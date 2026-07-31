#include <utility>

#include "backend/font/composite.h"
#include "backend/font/font.h"
#include "backend/font/registry.h"
#include "include/core/SkFontMgr.h"
#include "include/ports/SkFontMgr_android.h"
#include "include/ports/SkFontMgr_android_ndk.h"
#include "include/ports/SkFontMgr_data.h"
#include "include/ports/SkFontScanner_FreeType.h"
#include "utils/logger/logger.h"

namespace music_lyric_player::backend::font {
	namespace {
		constexpr utils::Logger logger{"AndroidFont"};

		/**
		 * Opens the platform font stack, preferring the NDK font API and falling back to the configuration file older releases ship.
		 */
		sk_sp<SkFontMgr> createSystemFontMgr() {
			// Files are left uncached so a family is mapped the first time something asks for it, rather than the whole system font set being read before the first frame.
			sk_sp<SkFontMgr> system = SkFontMgr_New_AndroidNDK(false, SkFontScanner_Make_FreeType());
			if (system != nullptr) {
				return system;
			}

			// The NDK font API arrived in Android 10, so anything older is read out of the fonts.xml the platform still ships.
			logger.info("the NDK font API is unavailable, reading the system font configuration instead");
			return SkFontMgr_New_Android(nullptr, SkFontScanner_Make_FreeType());
		}
	} // namespace

	sk_sp<SkFontMgr> createFontMgr() {
		sk_sp<SkFontMgr> system = createSystemFontMgr();
		if (system == nullptr) {
			logger.error("no system font stack could be opened");
		}

		// With nothing registered there is no second layer to build, and the system stack answers on its own.
		if (registeredFonts().empty()) {
			return system;
		}

		// The registered files go on top so a font the host supplied wins over a system family of the same name, while everything it does not carry still resolves.
		return createCompositeFontMgr(SkFontMgr_New_Custom_Data(registeredFonts()), std::move(system));
	}
} // namespace music_lyric_player::backend::font
