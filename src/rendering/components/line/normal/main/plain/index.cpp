#include "rendering/components/line/normal/main/plain/index.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMetrics.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkFontTypes.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPoint.h"
#include "include/core/SkScalar.h"
#include "include/core/SkTextBlob.h"
#include "include/core/SkTypeface.h"
#include "modules/skshaper/include/SkShaper.h"
#include "modules/skshaper/include/SkShaper_harfbuzz.h"
#include "modules/skshaper/include/SkShaper_skunicode.h"
#include "modules/skunicode/include/SkUnicode.h"
#include "music_lyric_model.h"
#include "rendering/common/context.h"
#include "rendering/utils/length.h"

namespace music_lyric_player::rendering::components::line::normal::main::plain {
	namespace {
		using Align = config::layout::Align;

		/**
		 * Builds the plain body font with baseline snapping disabled so it matches the timed word path.
		 * Per-glyph fallback inherits the same flags, so minority-script glyphs share the same metrics.
		 */
		SkFont buildPlainFont(const common::RenderContext& context, float size) {
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

		/**
		 * Computes the horizontal alignment offset that shifts a shaped line of the given width inside the block.
		 */
		float alignOffset(Align align, float blockWidth, float lineWidth) {
			switch (align) {
			case Align::Center:
				return (blockWidth - lineWidth) * 0.5f;
			case Align::Right:
				return blockWidth - lineWidth;
			case Align::Left:
			default:
				return 0.0f;
			}
		}

		/**
		 * One shaped and wrapped line: its blob plus the alignment offset resolved from its measured width.
		 */
		struct ShapedLine {
			sk_sp<SkTextBlob> blob;
			float             offsetX = 0.0f;
		};

		/**
		 * Shapes a width-wrapped run stream into one text blob per line while resolving each line's alignment.
		 * It mirrors Skia's own blob run handler but advances the baseline per line and records the block height.
		 */
		class MultilineBlobRunHandler final : public SkShaper::RunHandler {
		public:
			/**
			 * Caches the source utf8 plus the block width and alignment used to place each finished line.
			 */
			MultilineBlobRunHandler(const char* utf8, float blockWidth, Align align)
			    : utf8(utf8),
			      blockWidth(blockWidth),
			      align(align) {}

			/**
			 * Hands over the shaped lines once shaping has finished.
			 */
			std::vector<ShapedLine> takeLines() {
				return std::move(this->lines);
			}

			/**
			 * Returns the total block height in logical pixels across every wrapped line.
			 */
			float height() const {
				return this->cursorTop;
			}

			void beginLine() override {
				this->position    = {0.0f, this->cursorTop};
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
				this->position.fY = this->cursorTop - this->lineAscent;
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
			}

			void commitLine() override {
				const float lineWidth = this->position.fX;
				this->lines.push_back({this->builder.make(), alignOffset(this->align, this->blockWidth, lineWidth)});
				this->cursorTop += this->lineDescent - this->lineAscent;
			}

		private:
			SkTextBlobBuilder       builder;
			const char*             utf8       = nullptr;
			float                   blockWidth = 0.0f;
			Align                   align      = Align::Left;
			std::vector<ShapedLine> lines;
			uint32_t*               clusters      = nullptr;
			int                     glyphCount    = 0;
			int                     clusterOffset = 0;
			SkPoint                 position      = {0.0f, 0.0f};
			float                   lineAscent    = 0.0f;
			float                   lineDescent   = 0.0f;
			float                   cursorTop     = 0.0f;
		};
	} // namespace

	Element::Element(const music_lyric_model::parsed::Line& info)
	    : text(music_lyric_model::parsed::getParsedLineText(info)) {}

	Element::~Element() = default;

	void Element::layout(float width, const common::RenderContext& context) {
		this->width          = std::max(width, 1.0f);
		this->measuredHeight = 0.0f;
		this->lines.clear();

		if (!context.shaper || !context.fontMgr || !context.unicode || this->text.empty()) {
			return;
		}

		const config::Root& cfg  = context.config;
		const float         size = static_cast<float>(std::max(resolveLength(cfg.line.normal.main.syllable.font.size, config::Default.line.normal.main.syllable.font.size), 1.0));
		const SkFont        font = buildPlainFont(context, size);

		const char*       utf8  = this->text.c_str();
		const std::size_t bytes = this->text.size();

		std::unique_ptr<SkShaper::FontRunIterator>     fontIter   = SkShaper::MakeFontMgrRunIterator(utf8, bytes, font, context.fontMgr);
		std::unique_ptr<SkShaper::BiDiRunIterator>     bidiIter   = SkShapers::unicode::BidiRunIterator(context.unicode, utf8, bytes, 0);
		std::unique_ptr<SkShaper::ScriptRunIterator>   scriptIter = SkShapers::HB::ScriptRunIterator(utf8, bytes);
		std::unique_ptr<SkShaper::LanguageRunIterator> langIter   = std::make_unique<SkShaper::TrivialLanguageRunIterator>("", bytes);
		if (!fontIter || !bidiIter || !scriptIter || !langIter) {
			return;
		}

		MultilineBlobRunHandler handler(utf8, this->width, cfg.layout.align);
		context.shaper->shape(utf8, bytes, *fontIter, *bidiIter, *scriptIter, *langIter, this->width, &handler);

		std::vector<ShapedLine> shaped = handler.takeLines();
		this->lines.reserve(shaped.size());
		for (ShapedLine& line : shaped) {
			this->lines.push_back({std::move(line.blob), line.offsetX});
		}
		this->measuredHeight = handler.height();
	}

	void Element::paint(SkCanvas* canvas, float x, float y, SkColor color) const {
		if (this->lines.empty()) {
			return;
		}

		SkPaint paint;
		paint.setAntiAlias(true);
		paint.setColor(color);
		for (const PaintLine& line : this->lines) {
			if (line.blob) {
				canvas->drawTextBlob(line.blob, x + line.offsetX, y, paint);
			}
		}
	}

	float Element::height() const {
		return this->measuredHeight;
	}
} // namespace music_lyric_player::rendering::components::line::normal::main::plain
