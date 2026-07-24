#ifndef MUSIC_LYRIC_PLAYER_RENDERING_UTILS_SHAPING_ITERATOR_H_
#define MUSIC_LYRIC_PLAYER_RENDERING_UTILS_SHAPING_ITERATOR_H_

#include <cstddef>
#include <memory>

#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkRefCnt.h"
#include "modules/skshaper/include/SkShaper.h"
#include "modules/skshaper/include/SkShaper_harfbuzz.h"
#include "modules/skshaper/include/SkShaper_skunicode.h"
#include "modules/skunicode/include/SkUnicode.h"

namespace music_lyric_player::rendering::utils::shaping {
	/**
	 * The four run iterators SkShaper::shape needs, bundled so every text path builds them the same way.
	 */
	struct ShapingIterators {
		std::unique_ptr<SkShaper::FontRunIterator>     font;
		std::unique_ptr<SkShaper::BiDiRunIterator>     bidi;
		std::unique_ptr<SkShaper::ScriptRunIterator>   script;
		std::unique_ptr<SkShaper::LanguageRunIterator> language;

		/**
		 * Reports whether every iterator was constructed, so callers can bail before shaping.
		 */
		explicit operator bool() const {
			return this->font && this->bidi && this->script && this->language;
		}
	};

	/**
	 * Builds the standard run iterators: font-manager fallback, ICU BiDi, HarfBuzz script, and a trivial language.
	 * The empty language is a known limitation; section 5.2 of the text-layout-foundation plan replaces it with the resolved language.
	 */
	inline ShapingIterators makeShapingIterators(const sk_sp<SkUnicode>& unicode, const sk_sp<SkFontMgr>& fontMgr, const SkFont& font, const char* utf8, std::size_t bytes) {
		return ShapingIterators{
			SkShaper::MakeFontMgrRunIterator(utf8, bytes, font, fontMgr),
			SkShapers::unicode::BidiRunIterator(unicode, utf8, bytes, 0),
			SkShapers::HB::ScriptRunIterator(utf8, bytes),
			std::make_unique<SkShaper::TrivialLanguageRunIterator>("", bytes),
		};
	}
} // namespace music_lyric_player::rendering::utils::shaping

#endif // MUSIC_LYRIC_PLAYER_RENDERING_UTILS_SHAPING_ITERATOR_H_
