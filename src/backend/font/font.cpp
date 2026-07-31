#include "backend/font/font.h"

#include <utility>
#include <vector>

#include "backend/font/composite.h"
#include "backend/font/platform.h"
#include "include/core/SkData.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkSpan.h"

namespace music_lyric_player::backend::font {
	namespace {
		/**
		 * Holds every registered font file for the life of the process, outliving each font manager built from it.
		 */
		std::vector<sk_sp<SkData>>& registry() {
			static std::vector<sk_sp<SkData>> entries;
			return entries;
		}
	} // namespace

	bool registerFontData(sk_sp<SkData> data) {
		if (!data || data->isEmpty()) {
			return false;
		}

		// The same file fetched twice arrives as two separate blobs, and handing both over would leave a lookup choosing between two identical families.
		// SkData compares sizes before bytes, so this stays cheap for the usual case of distinct fonts.
		for (const sk_sp<SkData>& entry : registry()) {
			if (entry->equals(data.get())) {
				return false;
			}
		}

		registry().push_back(std::move(data));
		return true;
	}

	sk_sp<SkFontMgr> createFontMgr() {
		// Every registered file goes into a single layer rather than one layer each, because a lookup stops at the first layer carrying the family name.
		// Split across layers, a family's bold would sit below its regular and never be reached.
		sk_sp<SkFontMgr> registered = registry().empty() ? nullptr : createDataFontMgr(SkSpan<sk_sp<SkData>>(registry()));

		// The registered layer sits on top, so a font the host supplied shadows the system family of that name while everything it does not carry still resolves.
		return createCompositeFontMgr({std::move(registered), createSystemFontMgr()});
	}
} // namespace music_lyric_player::backend::font
