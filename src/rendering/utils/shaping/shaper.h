#ifndef MUSIC_LYRIC_PLAYER_RENDERING_UTILS_SHAPING_SHAPER_H_
#define MUSIC_LYRIC_PLAYER_RENDERING_UTILS_SHAPING_SHAPER_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

#include "include/core/SkFont.h"
#include "include/core/SkFontMetrics.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkPoint.h"
#include "include/core/SkRefCnt.h"
#include "include/core/SkTextBlob.h"
#include "include/core/SkTypes.h"
#include "modules/skshaper/include/SkShaper.h"
#include "modules/skunicode/include/SkUnicode.h"

#include "rendering/utils/shaping/glyph.h"
#include "rendering/utils/shaping/iterator.h"

namespace music_lyric_player::rendering::utils::shaping {
	namespace detail {
		/**
		 * Captures a shaper's full per-glyph output — id, absolute position, and absolute cluster — into a ShapedText.
		 * It reproduces the baseline and per-line advance the blob run handlers computed, so a rebuilt blob is identical.
		 * The empty-language and single-shaper choices live in the iterators; this handler only records what shaping produced.
		 */
		class CaptureRunHandler final : public SkShaper::RunHandler {
		public:
			/**
			 * Hands over the accumulated lines once shaping has finished.
			 */
			ShapedText take() {
				return std::move(this->result);
			}

			void beginLine() override {
				this->line        = ShapedLine{};
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
				this->glyphs.resize(static_cast<std::size_t>(glyphCount));
				this->positions.resize(static_cast<std::size_t>(glyphCount));
				this->clusters.resize(static_cast<std::size_t>(glyphCount));
				this->pendingGlyphCount = glyphCount;

				ShapedRun run;
				run.font      = info.fFont;
				run.utf8Begin = info.utf8Range.begin();
				run.utf8Size  = info.utf8Range.size();
				this->line.runs.push_back(std::move(run));

				return {this->glyphs.data(), this->positions.data(), nullptr, this->clusters.data(), this->position};
			}

			void commitRunBuffer(const RunInfo& info) override {
				ShapedRun& run = this->line.runs.back();
				run.glyphs.reserve(static_cast<std::size_t>(this->pendingGlyphCount));
				for (int i = 0; i < this->pendingGlyphCount; ++i) {
					run.glyphs.push_back(ShapedGlyph{this->glyphs[i], this->positions[i], this->clusters[i]});
				}
				this->position += info.fAdvance;
			}

			void commitLine() override {
				this->line.width   = this->position.fX;
				this->line.ascent  = -this->lineAscent;
				this->line.descent = this->lineDescent;
				this->cursorTop += this->lineDescent - this->lineAscent;
				this->result.lines.push_back(std::move(this->line));
			}

		private:
			ShapedText             result;
			ShapedLine             line;
			std::vector<SkGlyphID> glyphs;
			std::vector<SkPoint>   positions;
			std::vector<uint32_t>  clusters;
			int                    pendingGlyphCount = 0;
			SkPoint                position          = {0.0f, 0.0f};
			float                  lineAscent        = 0.0f;
			float                  lineDescent       = 0.0f;
			float                  cursorTop         = 0.0f;
		};
	} // namespace detail

	/**
	 * Shapes a utf8 string into a ShapedText, wrapping at `width` (pass a large value for a single unbounded line).
	 * Returns an empty result when the run iterators cannot be built, matching the callers' pre-shaping bail.
	 */
	inline ShapedText shapeText(SkShaper& shaper, const sk_sp<SkUnicode>& unicode, const sk_sp<SkFontMgr>& fontMgr, const SkFont& font, const char* utf8, std::size_t bytes, float width) {
		ShapingIterators iterators = makeShapingIterators(unicode, fontMgr, font, utf8, bytes);
		if (!iterators) {
			return {};
		}

		detail::CaptureRunHandler handler;
		shaper.shape(utf8, bytes, *iterators.font, *iterators.bidi, *iterators.script, *iterators.language, width, &handler);
		return handler.take();
	}

	/**
	 * Appends one shaped line's runs to a blob builder, copying each run's cluster text and rebasing clusters to run-local.
	 * Positions are written verbatim, so the resulting blob matches what the original blob run handlers produced.
	 */
	inline void appendLine(SkTextBlobBuilder& builder, const ShapedLine& line, const char* utf8) {
		for (const ShapedRun& run : line.runs) {
			const int glyphCount = static_cast<int>(run.glyphs.size());
			const int textCount  = static_cast<int>(run.utf8Size);

			const SkTextBlobBuilder::RunBuffer& buffer = builder.allocRunTextPos(run.font, glyphCount, textCount);
			if (buffer.utf8text && utf8) {
				std::memcpy(buffer.utf8text, utf8 + run.utf8Begin, static_cast<std::size_t>(textCount));
			}

			SkPoint* points = buffer.points();
			for (int i = 0; i < glyphCount; ++i) {
				const ShapedGlyph& glyph = run.glyphs[static_cast<std::size_t>(i)];
				buffer.glyphs[i]   = glyph.glyph;
				points[i]          = glyph.position;
				buffer.clusters[i] = glyph.cluster - static_cast<uint32_t>(run.utf8Begin);
			}
		}
	}
} // namespace music_lyric_player::rendering::utils::shaping

#endif // MUSIC_LYRIC_PLAYER_RENDERING_UTILS_SHAPING_SHAPER_H_
