#include <cstddef>
#include <utility>
#include <vector>

#include "backend/font/font.h"
#include "backend/font/registry.h"
#include "include/core/SkData.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkSpan.h"
#include "include/ports/SkFontMgr_data.h"

namespace music_lyric_player::backend::font {
	namespace {
		/**
		 * Holds every registered font file for the life of the process, outliving each font manager built from it.
		 */
		std::vector<sk_sp<SkData>>& registry() {
			static std::vector<sk_sp<SkData>> entries;
			return entries;
		}

		/**
		 * Counts changes to the registry, so a stale font manager can be spotted without comparing contents.
		 */
		std::size_t& generation() {
			static std::size_t count = 0;
			return count;
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
		++generation();
		return true;
	}

	std::size_t fontGeneration() {
		return generation();
	}

	sk_sp<SkFontMgr> createFontMgr() {
		// The browser exposes no system font stack, so families can only come from data registered at runtime.
		// The result is immutable, which is why a later registration builds a new manager instead of mutating this one.
		return SkFontMgr_New_Custom_Data(registry());
	}
} // namespace music_lyric_player::backend::font
