#include "rendering/components/line/normal/main/syllable/word.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMetrics.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkFontTypes.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPoint.h"
#include "include/core/SkRect.h"
#include "include/core/SkScalar.h"
#include "include/core/SkTextBlob.h"
#include "include/core/SkTypeface.h"
#include "modules/skshaper/include/SkShaper.h"
#include "modules/skshaper/include/SkShaper_harfbuzz.h"
#include "modules/skshaper/include/SkShaper_skunicode.h"
#include "modules/skunicode/include/SkUnicode.h"
#include "music_lyric_model.h"
#include "rendering/common/context.h"
#include "rendering/utils/color/parse.h"
#include "rendering/utils/length.h"

namespace music_lyric_player::rendering::components::line::normal::main::syllable {
	namespace {
		constexpr float kGlyphOutset    = 2.0f;
		constexpr float kUnboundedWidth = 1'000'000.0f;
		constexpr float kWidthEpsilon   = 0.01f;

		/**
		 * Returns the word's absolute start time, or zero when timing is absent.
		 */
		double wordStart(const ::lyric::runtime::WordNormal& info) {
			const ::lyric::common::Time* time = ::music_lyric_model::runtime::getWordTime(info);
			return time ? static_cast<double>(time->start()) : 0.0;
		}

		/**
		 * Returns the word's non-negative duration.
		 */
		double wordDuration(const ::lyric::runtime::WordNormal& info) {
			return std::max(static_cast<double>(::music_lyric_model::runtime::getWordDuration(info)), 0.0);
		}

		/**
		 * Shapes one run stream into a single text blob while capturing its advance width and line metrics.
		 * It mirrors Skia's own blob run handler but also reports width and typographic ascent / descent for layout.
		 */
		class BlobRunHandler final : public SkShaper::RunHandler {
		public:
			/**
			 * Caches the source utf8 so run buffers can copy their per-glyph cluster text.
			 */
			explicit BlobRunHandler(const char* utf8)
			    : utf8(utf8) {}

			/**
			 * Builds the accumulated text blob once shaping has finished.
			 */
			sk_sp<SkTextBlob> makeBlob() {
				return this->builder.make();
			}

			/**
			 * Returns the shaped advance width in logical pixels.
			 */
			float width() const {
				return this->maxWidth;
			}

			/**
			 * Returns the distance from the top to the baseline in logical pixels.
			 */
			float ascent() const {
				return -this->lineAscent;
			}

			/**
			 * Returns the distance from the baseline to the bottom in logical pixels.
			 */
			float descent() const {
				return this->lineDescent;
			}

			void beginLine() override {
				this->position    = {0.0f, 0.0f};
				this->lineAscent  = 0.0f;
				this->lineDescent = 0.0f;
			}

			void runInfo(const RunInfo& info) override {
				SkFontMetrics metrics;
				info.fFont.getMetrics(&metrics);
				this->lineAscent  = std::min(this->lineAscent, metrics.fAscent);
				this->lineDescent = std::max(this->lineDescent, metrics.fDescent);
			}

			void commitRunInfo() override {
				this->position.fY -= this->lineAscent;
			}

			Buffer runBuffer(const RunInfo& info) override {
				const int glyphCount = static_cast<int>(info.glyphCount);
				const int textCount  = static_cast<int>(info.utf8Range.size());

				const SkTextBlobBuilder::RunBuffer& run = this->builder.allocRunTextPos(info.fFont, glyphCount, textCount);
				if (run.utf8text && this->utf8) {
					std::memcpy(run.utf8text, this->utf8 + info.utf8Range.begin(), static_cast<std::size_t>(textCount));
				}
				this->clusters      = run.clusters;
				this->glyphCount    = glyphCount;
				this->clusterOffset = static_cast<int>(info.utf8Range.begin());
				return {run.glyphs, run.points(), nullptr, run.clusters, this->position};
			}

			void commitRunBuffer(const RunInfo& info) override {
				for (int i = 0; i < this->glyphCount; ++i) {
					this->clusters[i] -= static_cast<uint32_t>(this->clusterOffset);
				}
				this->position += info.fAdvance;
				this->maxWidth = std::max(this->maxWidth, this->position.fX);
			}

			void commitLine() override {}

		private:
			SkTextBlobBuilder builder;
			const char*       utf8          = nullptr;
			uint32_t*         clusters      = nullptr;
			int               glyphCount    = 0;
			int               clusterOffset = 0;
			SkPoint           position      = {0.0f, 0.0f};
			float             maxWidth      = 0.0f;
			float             lineAscent    = 0.0f;
			float             lineDescent   = 0.0f;
		};

		/**
		 * Builds the primary word font with baseline snapping disabled so a floating word moves smoothly sub-pixel.
		 * Per-glyph fallback keeps the same flags, so minority-script glyphs also escape the vertical pixel grid.
		 */
		SkFont buildWordFont(const common::RenderContext& context, float size) {
			const config::Root& cfg = context.config;

			sk_sp<SkTypeface> typeface;
			if (!cfg.line.normal.main.syllable.font.family.empty()) {
				typeface = context.fontMgr->matchFamilyStyle(cfg.line.normal.main.syllable.font.family.c_str(), SkFontStyle::Normal());
			}
			if (!typeface) {
				typeface = context.fontMgr->matchFamilyStyle(nullptr, SkFontStyle::Normal());
			}

			SkFont font(typeface, static_cast<SkScalar>(size));
			font.setSubpixel(true);
			font.setBaselineSnap(false);
			font.setEdging(SkFont::Edging::kAntiAlias);
			font.setHinting(SkFontHinting::kNone);
			return font;
		}
	} // namespace

	Word::Word(const ::lyric::runtime::WordNormal& info, bool hasSpaceBefore)
	    : text(info.content()),
	      start(wordStart(info)),
	      duration(wordDuration(info)),
	      spaceBefore(hasSpaceBefore),
	      floating(this->start, this->duration) {}

	Word::~Word() = default;

	void Word::layout(const common::RenderContext& context) {
		this->measuredWidth    = 0.0f;
		this->measuredHeight   = 0.0f;
		this->measuredBaseline = 0.0f;
		this->blob             = nullptr;

		if (!context.shaper || !context.fontMgr || this->text.empty()) {
			return;
		}

		const config::Root& cfg  = context.config;
		const float         size = static_cast<float>(std::max(resolveLength(cfg.line.normal.main.syllable.font.size, config::Default.line.normal.main.syllable.font.size), 1.0));
		const SkFont        font = buildWordFont(context, size);

		const char*       utf8  = this->text.c_str();
		const std::size_t bytes = this->text.size();

		std::unique_ptr<SkShaper::FontRunIterator>     fontIter   = SkShaper::MakeFontMgrRunIterator(utf8, bytes, font, context.fontMgr);
		std::unique_ptr<SkShaper::BiDiRunIterator>     bidiIter   = SkShapers::unicode::BidiRunIterator(context.unicode, utf8, bytes, 0);
		std::unique_ptr<SkShaper::ScriptRunIterator>   scriptIter = SkShapers::HB::ScriptRunIterator(utf8, bytes);
		std::unique_ptr<SkShaper::LanguageRunIterator> langIter   = std::make_unique<SkShaper::TrivialLanguageRunIterator>("", bytes);
		if (!fontIter || !bidiIter || !scriptIter || !langIter) {
			return;
		}

		BlobRunHandler handler(utf8);
		context.shaper->shape(utf8, bytes, *fontIter, *bidiIter, *scriptIter, *langIter, kUnboundedWidth, &handler);

		this->blob             = handler.makeBlob();
		this->measuredWidth    = std::ceil(std::max(handler.width(), 1.0f) + kWidthEpsilon);
		this->measuredBaseline = handler.ascent();
		this->measuredHeight   = handler.ascent() + handler.descent();
	}

	void Word::setPosition(float x, float y) {
		this->x = x;
		this->y = y;
	}

	void Word::paintBlob(SkCanvas* canvas, float x, float y, SkColor color) const {
		if (!this->blob) {
			return;
		}
		SkPaint paint;
		paint.setAntiAlias(true);
		paint.setColor(color);
		canvas->drawTextBlob(this->blob, x, y, paint);
	}

	void Word::paintReveal(SkCanvas* canvas, float x, float y, float progress, float feather, SkColor unsungColor, SkColor sungColor) const {
		if (!this->blob) {
			return;
		}

		const SkRect textBounds = SkRect::MakeXYWH(x, y, this->measuredWidth, this->measuredHeight);
		const SkRect drawBounds = textBounds.makeOutset(kGlyphOutset, kGlyphOutset);
		canvas->saveLayer(&drawBounds, nullptr);
		// Opaque coverage: the mask supplies the alpha, so the base glyph layer stays fully opaque here.
		this->paintBlob(canvas, x, y, SK_ColorWHITE);
		animation::Mask::apply(canvas, drawBounds, textBounds, progress, feather, unsungColor, sungColor);
		canvas->restore();
	}

	void Word::paint(SkCanvas* canvas, float lineX, float lineY, double now, bool active, bool maskEnabled, float maskProgress, float maskFeather, const common::RenderContext& context) const {
		if (!this->blob) {
			return;
		}

		const config::Root& cfg             = context.config;
		const auto&         animationConfig = cfg.line.normal.main.syllable.word.animation;
		const double        fromValue       = std::isfinite(animationConfig.floating.from)
			      ? animationConfig.floating.from
			      : config::Default.line.normal.main.syllable.word.animation.floating.from;
		const double        toValue         = std::isfinite(animationConfig.floating.to)
			      ? animationConfig.floating.to
			      : config::Default.line.normal.main.syllable.word.animation.floating.to;
		const float         offset          = this->floating.sample(context.currentTime, now, active, animationConfig.floating.enabled, static_cast<float>(fromValue), static_cast<float>(toValue));
		const float         drawX           = lineX + this->x;
		const float         drawY           = lineY + this->y + offset;

		const SkColor normalColor    = utils::color::resolve(cfg.line.normal.main.syllable.style.normal.color, config::Default.line.normal.main.syllable.style.normal.color);
		const SkColor activeColor    = utils::color::resolve(cfg.line.normal.main.syllable.style.active.color, config::Default.line.normal.main.syllable.style.active.color);
		const double  normalOpacity  = cfg.line.normal.main.syllable.style.normal.opacity;
		const double  activeOpacity  = cfg.line.normal.main.syllable.style.active.opacity;

		// Inactive lines paint the whole word in the normal state color and opacity.
		if (!active) {
			this->paintBlob(canvas, drawX, drawY, utils::color::withOpacity(normalColor, normalOpacity));
			return;
		}

		// The active line switches the rgb to the active color at once; the mask only wipes alpha, from the normal opacity (unsung) up to the active opacity (sung), so the hue never changes.
		const SkColor sungColor   = utils::color::withOpacity(activeColor, activeOpacity);
		const SkColor unsungColor = utils::color::withOpacity(activeColor, normalOpacity);

		if (!maskEnabled || maskProgress >= 1.0f) {
			this->paintBlob(canvas, drawX, drawY, sungColor);
			return;
		}
		if (maskProgress <= 0.0f) {
			this->paintBlob(canvas, drawX, drawY, unsungColor);
			return;
		}
		this->paintReveal(canvas, drawX, drawY, maskProgress, maskFeather, unsungColor, sungColor);
	}

	animation::Mask::Input Word::maskInput() const {
		return animation::Mask::Input{this->start, this->duration, this->measuredWidth, this->measuredHeight};
	}

	bool Word::hasSpaceBefore() const {
		return this->spaceBefore;
	}

	float Word::width() const {
		return this->measuredWidth;
	}

	float Word::height() const {
		return this->measuredHeight;
	}

	float Word::baseline() const {
		return this->measuredBaseline;
	}
} // namespace music_lyric_player::rendering::components::line::normal::main::syllable
