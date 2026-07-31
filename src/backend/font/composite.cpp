#include "backend/font/composite.h"

#include <memory>
#include <utility>

#include "include/core/SkData.h"
#include "include/core/SkFontArguments.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkStream.h"
#include "include/core/SkTypeface.h"
#include "include/core/SkTypes.h"

namespace music_lyric_player::backend::font {
	namespace {
		/**
		 * A font manager laying one manager over another, answering from the overlay and falling through to the base.
		 * Only the public entry points of the two are reachable from here, since each manager's `on*` overrides are protected to its own type; that costs nothing, as every public one is a thin forward to its override.
		 */
		class CompositeFontMgr : public SkFontMgr {
		public:
			/**
			 * Composes `overlay` over `base`, holding a reference to each for as long as this manager lives.
			 */
			CompositeFontMgr(sk_sp<SkFontMgr> overlay, sk_sp<SkFontMgr> base) : overlay(std::move(overlay)), base(std::move(base)) {}

		protected:
			/**
			 * Returns the two family lists laid end to end.
			 */
			int onCountFamilies() const override {
				return this->overlay->countFamilies() + this->base->countFamilies();
			}

			/**
			 * Names the family at `index`, which belongs to the base once it runs past the overlay's own count.
			 */
			void onGetFamilyName(int index, SkString* familyName) const override {
				const int overlayCount = this->overlay->countFamilies();
				if (index < overlayCount) {
					this->overlay->getFamilyName(index, familyName);
					return;
				}
				this->base->getFamilyName(index - overlayCount, familyName);
			}

			/**
			 * Opens the style set at `index`, split between the two the same way the names are.
			 */
			sk_sp<SkFontStyleSet> onCreateStyleSet(int index) const override {
				const int overlayCount = this->overlay->countFamilies();
				if (index < overlayCount) {
					return this->overlay->createStyleSet(index);
				}
				return this->base->createStyleSet(index - overlayCount);
			}

			/**
			 * Resolves `familyName` against the overlay, then the base.
			 * A miss has to be read off the count, because the public entry point turns the null its override returns into an empty set.
			 */
			sk_sp<SkFontStyleSet> onMatchFamily(const char familyName[]) const override {
				sk_sp<SkFontStyleSet> matched = this->overlay->matchFamily(familyName);
				if (matched->count() > 0) {
					return matched;
				}
				return this->base->matchFamily(familyName);
			}

			/**
			 * Picks the closest face to `style` within `familyName`, from the overlay when it carries that family and from the base otherwise.
			 */
			sk_sp<SkTypeface> onMatchFamilyStyle(const char familyName[], const SkFontStyle& style) const override {
				sk_sp<SkTypeface> matched = this->overlay->matchFamilyStyle(familyName, style);
				return matched != nullptr ? matched : this->base->matchFamilyStyle(familyName, style);
			}

			/**
			 * Finds any face covering `character`, asking both layers.
			 * This is the per-character fallback a shaper reaches for once a run has no typeface, so stopping at the overlay would drop every character the registered fonts do not cover, which on a CJK or emoji run is most of them.
			 */
			sk_sp<SkTypeface> onMatchFamilyStyleCharacter(const char familyName[],
				const SkFontStyle&                               style,
				const char*                                      bcp47[],
				int                                              bcp47Count,
				SkUnichar                                        character) const override {
				sk_sp<SkTypeface> matched = this->overlay->matchFamilyStyleCharacter(familyName, style, bcp47, bcp47Count, character);
				return matched != nullptr ? matched : this->base->matchFamilyStyleCharacter(familyName, style, bcp47, bcp47Count, character);
			}

			/**
			 * Builds a face out of `data`, which is the overlay's job: it is the manager made out of font data in the first place, while the base speaks for what the platform already has installed.
			 */
			sk_sp<SkTypeface> onMakeFromData(sk_sp<SkData> data, int ttcIndex) const override {
				return this->overlay->makeFromData(std::move(data), ttcIndex);
			}

			/**
			 * Builds a face out of `stream`, taking the face at `ttcIndex` in a collection.
			 */
			sk_sp<SkTypeface> onMakeFromStreamIndex(std::unique_ptr<SkStreamAsset> stream, int ttcIndex) const override {
				return this->overlay->makeFromStream(std::move(stream), ttcIndex);
			}

			/**
			 * Builds a face out of `stream` with `args` selecting the instance of a variable font.
			 */
			sk_sp<SkTypeface> onMakeFromStreamArgs(std::unique_ptr<SkStreamAsset> stream, const SkFontArguments& args) const override {
				return this->overlay->makeFromStream(std::move(stream), args);
			}

			/**
			 * Builds a face out of the file at `path`.
			 */
			sk_sp<SkTypeface> onMakeFromFile(const char path[], int ttcIndex) const override {
				return this->overlay->makeFromFile(path, ttcIndex);
			}

			/**
			 * Answers the legacy request that must not come back empty handed.
			 * The overlay is asked through the matching entry point rather than its own legacy one, which never fails and so would answer for system families too; a null family name asks for the default font, which only the base can speak for.
			 */
			sk_sp<SkTypeface> onLegacyMakeTypeface(const char familyName[], SkFontStyle style) const override {
				if (familyName != nullptr) {
					sk_sp<SkTypeface> matched = this->overlay->matchFamilyStyle(familyName, style);
					if (matched != nullptr) {
						return matched;
					}
				}
				return this->base->legacyMakeTypeface(familyName, style);
			}

		private:
			sk_sp<SkFontMgr> overlay;
			sk_sp<SkFontMgr> base;
		};
	} // namespace

	sk_sp<SkFontMgr> createCompositeFontMgr(sk_sp<SkFontMgr> overlay, sk_sp<SkFontMgr> base) {
		if (overlay == nullptr) {
			return base;
		}
		if (base == nullptr) {
			return overlay;
		}
		return sk_make_sp<CompositeFontMgr>(std::move(overlay), std::move(base));
	}
} // namespace music_lyric_player::backend::font
