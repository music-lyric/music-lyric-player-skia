#include "backend/font/composite.h"

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

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
		 * A font manager stacking several others, answering from the topmost layer that can and falling through to the next.
		 * Only the public entry points of the layers are reachable from here, since each manager's `on*` overrides are protected to its own type.
		 * That costs nothing, as every public one is a thin forward to its override.
		 */
		class CompositeFontMgr : public SkFontMgr {
		public:
			/**
			 * Stacks `layers` top first, holding a reference to each and taking their family counts as fixed.
			 */
			explicit CompositeFontMgr(std::vector<sk_sp<SkFontMgr>> layers) : layers(std::move(layers)) {
				// Each entry is where that layer's families start in the concatenated list, and the closing one is the total.
				// Counting once is what keeps indexed access from walking a chain of `countFamilies()` calls, and it holds because a font manager never gains or loses a family.
				this->familyStarts.reserve(this->layers.size() + 1);
				this->familyStarts.push_back(0);
				for (const sk_sp<SkFontMgr>& layer : this->layers) {
					this->familyStarts.push_back(this->familyStarts.back() + layer->countFamilies());
				}
			}

		protected:
			/**
			 * Returns how many families the layers hold between them.
			 */
			int onCountFamilies() const override {
				return this->familyStarts.back();
			}

			/**
			 * Names the family at `index` in the concatenated list, leaving `familyName` untouched when the index names none.
			 */
			void onGetFamilyName(int index, SkString* familyName) const override {
				const std::size_t layer = layerOf(index);
				if (layer == this->layers.size()) {
					return;
				}
				this->layers[layer]->getFamilyName(index - this->familyStarts[layer], familyName);
			}

			/**
			 * Opens the style set of the family at `index`, split across the layers the same way the names are.
			 */
			sk_sp<SkFontStyleSet> onCreateStyleSet(int index) const override {
				const std::size_t layer = layerOf(index);
				if (layer == this->layers.size()) {
					return nullptr;
				}
				return this->layers[layer]->createStyleSet(index - this->familyStarts[layer]);
			}

			/**
			 * Resolves `familyName` in the topmost layer that carries it.
			 * A miss has to be read off the count, because the public entry point turns the null its override returns into an empty set.
			 */
			sk_sp<SkFontStyleSet> onMatchFamily(const char familyName[]) const override {
				for (const sk_sp<SkFontMgr>& layer : this->layers) {
					sk_sp<SkFontStyleSet> matched = layer->matchFamily(familyName);
					if (matched->count() > 0) {
						return matched;
					}
				}
				return nullptr;
			}

			/**
			 * Picks the closest face to `style` within `familyName`, from the topmost layer that carries that family.
			 */
			sk_sp<SkTypeface> onMatchFamilyStyle(const char familyName[], const SkFontStyle& style) const override {
				for (const sk_sp<SkFontMgr>& layer : this->layers) {
					sk_sp<SkTypeface> matched = layer->matchFamilyStyle(familyName, style);
					if (matched != nullptr) {
						return matched;
					}
				}
				return nullptr;
			}

			/**
			 * Finds any face covering `character`, asking every layer rather than stopping at the first that knows the family.
			 * This is the per-character fallback a shaper reaches for once a run has no typeface of its own.
			 * Stopping early would drop every character the upper layers do not cover, which on a CJK or emoji run is most of them.
			 */
			sk_sp<SkTypeface> onMatchFamilyStyleCharacter(const char familyName[],
				const SkFontStyle&                               style,
				const char*                                      bcp47[],
				int                                              bcp47Count,
				SkUnichar                                        character) const override {
				for (const sk_sp<SkFontMgr>& layer : this->layers) {
					sk_sp<SkTypeface> matched = layer->matchFamilyStyleCharacter(familyName, style, bcp47, bcp47Count, character);
					if (matched != nullptr) {
						return matched;
					}
				}
				return nullptr;
			}

			/**
			 * Builds a face out of `data`, which the top layer answers alone.
			 * Parsing font bytes does not depend on which families a layer knows, so walking down the stack would only parse the same bytes again.
			 */
			sk_sp<SkTypeface> onMakeFromData(sk_sp<SkData> data, int ttcIndex) const override {
				return this->layers.front()->makeFromData(std::move(data), ttcIndex);
			}

			/**
			 * Builds a face out of `stream`, taking the face at `ttcIndex` in a collection.
			 */
			sk_sp<SkTypeface> onMakeFromStreamIndex(std::unique_ptr<SkStreamAsset> stream, int ttcIndex) const override {
				return this->layers.front()->makeFromStream(std::move(stream), ttcIndex);
			}

			/**
			 * Builds a face out of `stream` with `args` selecting the instance of a variable font.
			 */
			sk_sp<SkTypeface> onMakeFromStreamArgs(std::unique_ptr<SkStreamAsset> stream, const SkFontArguments& args) const override {
				return this->layers.front()->makeFromStream(std::move(stream), args);
			}

			/**
			 * Builds a face out of the file at `path`.
			 */
			sk_sp<SkTypeface> onMakeFromFile(const char path[], int ttcIndex) const override {
				return this->layers.front()->makeFromFile(path, ttcIndex);
			}

			/**
			 * Answers the legacy request that must not come back empty handed.
			 * Each layer is asked through its matching entry point rather than its own legacy one, which on a data-backed manager never fails and would therefore answer for system families too.
			 * Whatever is left over goes to the bottom layer, the one standing for the platform, and so does the null family name that asks for the default font.
			 */
			sk_sp<SkTypeface> onLegacyMakeTypeface(const char familyName[], SkFontStyle style) const override {
				if (familyName != nullptr) {
					for (const sk_sp<SkFontMgr>& layer : this->layers) {
						sk_sp<SkTypeface> matched = layer->matchFamilyStyle(familyName, style);
						if (matched != nullptr) {
							return matched;
						}
					}
				}
				return this->layers.back()->legacyMakeTypeface(familyName, style);
			}

		private:
			/**
			 * Returns which layer the family at `index` sits in, or the layer count when `index` names no family at all.
			 */
			std::size_t layerOf(int index) const {
				if (index < 0) {
					return this->layers.size();
				}
				for (std::size_t layer = 0; layer < this->layers.size(); ++layer) {
					if (index < this->familyStarts[layer + 1]) {
						return layer;
					}
				}
				return this->layers.size();
			}

			std::vector<sk_sp<SkFontMgr>> layers;
			std::vector<int>              familyStarts;
		};
	} // namespace

	sk_sp<SkFontMgr> createCompositeFontMgr(std::vector<sk_sp<SkFontMgr>> layers) {
		std::erase(layers, nullptr);

		if (layers.empty()) {
			return SkFontMgr::RefEmpty();
		}
		if (layers.size() == 1) {
			return std::move(layers.front());
		}
		return sk_make_sp<CompositeFontMgr>(std::move(layers));
	}
} // namespace music_lyric_player::backend::font
