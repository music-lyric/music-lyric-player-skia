#include "render/renderer.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "include/core/SkBlendMode.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkColorFilter.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkPaint.h"
#include "include/core/SkRect.h"
#include "include/core/SkScalar.h"
#include "include/core/SkString.h"
#include "info.pb.h"
#include "line/content.h"
#include "modules/skparagraph/include/DartTypes.h"
#include "modules/skparagraph/include/FontCollection.h"
#include "modules/skparagraph/include/Paragraph.h"
#include "modules/skparagraph/include/ParagraphBuilder.h"
#include "modules/skparagraph/include/ParagraphStyle.h"
#include "modules/skparagraph/include/TextStyle.h"
#include "modules/skunicode/include/SkUnicode.h"
#include "modules/skunicode/include/SkUnicode_icu.h"
#include "playback/player.h"
#include "render/utils/animation/easing.h"
#include "render/utils/length.h"

namespace tl = ::skia::textlayout;

namespace music_lyric_player::render {
	namespace {
		// Numeric fallbacks mirroring the string length defaults in the config schema,
		// used when a config value fails to parse.
		constexpr double kDefaultFontSize = 34.0; // line.font.size     ("34px")
		constexpr double kDefaultPaddingX = 48.0; // container.paddingX ("48px")
		constexpr double kDefaultGap      = 16.0; // layout.gap         ("16px")

		/**
		 * Maps the config's integer alignment onto SkParagraph's `TextAlign`.
		 */
		tl::TextAlign toTextAlign(int align) {
			switch (align) {
			case 1:
				return tl::TextAlign::kCenter;
			case 2:
				return tl::TextAlign::kRight;
			default:
				return tl::TextAlign::kLeft;
			}
		}
	} // namespace

	Renderer::Renderer(playback::Player& player, sk_sp<SkFontMgr> fontMgr, const Clock& clock)
	    : player_(player),
	      fontMgr_(std::move(fontMgr)),
	      clock_(clock) {
		// Resolve families from the system font manager, with per-glyph fallback so mixed scripts do not render as tofu.
		fonts_ = sk_make_sp<tl::FontCollection>();
		fonts_->setDefaultFontManager(fontMgr_);
		fonts_->enableFontFallback();

		// Unicode backend drives SkParagraph's word / grapheme / line-break boundaries.
		unicode_ = SkUnicodes::ICU::Make();

		// The scroll offset eases with a fixed cubic curve; its duration is refreshed from config each frame.
		scroll_.setEasing(animation::inOutCubic);

		lyricListener_  = player_.onLyricUpdate.add([this](const ::lyric::Info& info) {
			handleLyricUpdate(info);
		});
		linesListener_  = player_.onLinesUpdate.add([this](const std::vector<int>&, int firstIndex, bool) {
			handleLinesUpdate(firstIndex);
		});
		configListener_ = config.onUpdate.add([this](const config::ChangeKeys&, const config::Root&) {
			handleConfigUpdate();
		});

		// Adopt any lyric already loaded before the renderer attached.
		if (player_.currentInfo().lines_size() > 0) {
			rebuildLines(player_.currentInfo());
			activeIndex_ = player_.currentActive();
		}
	}

	Renderer::~Renderer() {
		dispose();
	}

	void Renderer::dispose() {
		if (lyricListener_ != 0) {
			player_.onLyricUpdate.remove(lyricListener_);
			lyricListener_ = 0;
		}
		if (linesListener_ != 0) {
			player_.onLinesUpdate.remove(linesListener_);
			linesListener_ = 0;
		}
		if (configListener_ != 0) {
			config.onUpdate.remove(configListener_);
			configListener_ = 0;
		}
		lines_.clear();
		activeIndex_ = -1;
	}

	void Renderer::setViewport(int widthPx, int heightPx, float dpr) {
		dpr = (std::isfinite(dpr) && dpr > 0.0f) ? dpr : 1.0f;
		if (widthPx == viewportW_ && heightPx == viewportH_ && dpr == dpr_) {
			return;
		}
		// A width or dpr change re-wraps every line; a height change only shifts scrolling.
		if (widthPx != viewportW_ || dpr != dpr_) {
			layoutDirty_ = true;
		}
		viewportW_ = std::max(widthPx, 0);
		viewportH_ = std::max(heightPx, 0);
		dpr_       = dpr;
	}

	void Renderer::handleLyricUpdate(const ::lyric::Info& info) {
		rebuildLines(info);
		activeIndex_ = -1;
	}

	void Renderer::handleLinesUpdate(int firstIndex) {
		activeIndex_ = firstIndex;
	}

	void Renderer::handleConfigUpdate() {
		layoutDirty_ = true;
	}

	void Renderer::rebuildLines(const ::lyric::Info& info) {
		lines_.clear();
		lines_.reserve(static_cast<std::size_t>(std::max(info.lines_size(), 0)));
		for (int i = 0; i < info.lines_size(); ++i) {
			const ::lyric::Line& line = info.lines(i);
			RenderLine           render;
			render.index     = i;
			render.interlude = ::music_lyric_model::isLineInterlude(line);
			render.text      = render.interlude ? std::string{} : ::music_lyric_model::getLineText(line);
			lines_.push_back(std::move(render));
		}
		// Drop the scroll tween so a freshly loaded lyric snaps into place instead of sliding from the old song.
		scrollInit_  = false;
		scrollFocus_ = -1;
		layoutDirty_ = true;
	}

	std::unique_ptr<tl::Paragraph> Renderer::buildParagraph(const std::string& text) const {
		if (!unicode_ || !fonts_) {
			return nullptr;
		}
		const config::Root& cfg = config.current();

		tl::TextStyle textStyle;
		textStyle.setColor(SK_ColorWHITE); // tinted per state at paint time via kModulate
		textStyle.setFontSize(static_cast<SkScalar>(resolveLength(cfg.line.font.size, kDefaultFontSize)));
		if (!cfg.line.font.family.empty()) {
			textStyle.setFontFamilies({SkString(cfg.line.font.family.c_str())});
		}
		textStyle.setFontStyle(SkFontStyle::Normal());

		tl::ParagraphStyle paraStyle;
		paraStyle.setTextStyle(textStyle);
		paraStyle.setTextAlign(toTextAlign(cfg.layout.align));

		std::unique_ptr<tl::ParagraphBuilder> builder = tl::ParagraphBuilder::make(paraStyle, fonts_, unicode_);
		if (!builder) {
			return nullptr;
		}
		builder->addText(text.c_str());
		return builder->Build();
	}

	void Renderer::ensureLayout(float contentWidth) {
		if (!layoutDirty_ && contentWidth == layoutWidth_) {
			return;
		}
		const float wrapWidth = std::max(contentWidth, 1.0f);
		for (RenderLine& line : lines_) {
			// Interlude lines have no renderer yet; lay them out empty so they occupy no paint.
			if (line.interlude) {
				line.paragraph = nullptr;
				line.height    = 0.0f;
				continue;
			}
			line.paragraph = buildParagraph(line.text);
			if (line.paragraph) {
				line.paragraph->layout(wrapWidth);
				line.height = line.paragraph->getHeight();
			} else {
				line.height = 0.0f;
			}
		}
		layoutDirty_ = false;
		layoutWidth_ = contentWidth;
	}

	void Renderer::render(SkCanvas* canvas) {
		if (canvas == nullptr) {
			return;
		}
		const config::Root& cfg = config.current();

		// Background always fills, even before a lyric loads.
		canvas->clear(static_cast<SkColor>(cfg.container.backgroundColor));

		if (viewportW_ <= 0 || viewportH_ <= 0) {
			return;
		}

		// Work in logical pixels; the device-pixel ratio is folded into the canvas transform.
		SkAutoCanvasRestore autoRestore(canvas, true);
		canvas->scale(dpr_, dpr_);
		const float logicalW = static_cast<float>(viewportW_) / dpr_;
		const float logicalH = static_cast<float>(viewportH_) / dpr_;

		const float padX         = std::min(static_cast<float>(resolveLength(cfg.container.paddingX, kDefaultPaddingX, logicalW)), logicalW * 0.5f);
		const float contentWidth = std::max(logicalW - 2.0f * padX, 1.0f);
		ensureLayout(contentWidth);

		if (lines_.empty()) {
			return;
		}

		const float gap = static_cast<float>(resolveLength(cfg.layout.gap, kDefaultGap, logicalH));

		// Stack each line below the previous plus the gap; no off-screen virtualization yet.
		std::vector<float> tops(lines_.size(), 0.0f);
		float              cursor = 0.0f;
		for (std::size_t i = 0; i < lines_.size(); ++i) {
			tops[i] = cursor;
			cursor += lines_[i].height + gap;
		}

		// Centre the focus (primary active) line on the anchor.
		const std::size_t focus       = (activeIndex_ >= 0 && activeIndex_ < static_cast<int>(lines_.size()))
			? static_cast<std::size_t>(activeIndex_)
			: 0;
		const float       anchorY     = logicalH * static_cast<float>(cfg.scroll.anchor);
		const float       focusCentre = tops[focus] + lines_[focus].height * 0.5f;
		const float       targetY     = focusCentre - anchorY;

		// Ease the scroll towards the focus line.
		// A focus change restarts the tween from the current position.
		// Under a stable focus the landing follows layout drift, snapping once the ease finishes.
		const double nowMs = clock_.nowMs();
		scroll_.setDuration(cfg.scroll.animation.duration);
		if (!scrollInit_) {
			scroll_.snap(targetY);
			scrollFocus_ = static_cast<int>(focus);
			scrollInit_  = true;
		} else if (static_cast<int>(focus) != scrollFocus_) {
			scroll_.retarget(nowMs, targetY);
			scrollFocus_ = static_cast<int>(focus);
		} else if (scroll_.finished(nowMs)) {
			scroll_.snap(targetY);
		} else {
			scroll_.setTarget(targetY);
		}
		const float scrollY = scroll_.sample(nowMs);

		for (std::size_t i = 0; i < lines_.size(); ++i) {
			RenderLine& line = lines_[i];
			if (!line.paragraph) {
				continue;
			}
			const float y = tops[i] - scrollY;
			// Cull lines fully outside the viewport.
			if (y + line.height < 0.0f || y > logicalH) {
				continue;
			}

			const bool    isActive = static_cast<int>(i) == activeIndex_;
			const SkColor color    = static_cast<SkColor>(isActive ? cfg.line.active.color : cfg.line.normal.color);

			// The paragraph is opaque white; a modulate layer tints it to the state colour without re-shaping.
			SkPaint layerPaint;
			layerPaint.setColorFilter(SkColorFilters::Blend(color, SkBlendMode::kModulate));
			const SkRect bounds = SkRect::MakeXYWH(padX, y, contentWidth, line.height);
			canvas->saveLayer(&bounds, &layerPaint);
			line.paragraph->paint(canvas, padX, y);
			canvas->restore();
		}
	}
} // namespace music_lyric_player::render
