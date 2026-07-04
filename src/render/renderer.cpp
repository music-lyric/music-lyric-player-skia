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

namespace tl = ::skia::textlayout;

namespace music_lyric_player::render {
	namespace {
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

		/**
		 * Cubic ease-in-out over a normalized 0..1 progress; the input is clamped to the range.
		 */
		float easeInOutCubic(float t) {
			t = std::clamp(t, 0.0f, 1.0f);
			return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) * 0.5f;
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

		lyricListener_  = player_.onLyricUpdate.add([this](const ::lyric::Info& info) {
			handleLyricUpdate(info);
		});
		linesListener_  = player_.onLinesUpdate.add([this](const std::vector<int>&, int firstIndex, bool) {
			handleLinesUpdate(firstIndex);
		});
		configListener_ = config.onUpdate.add([this](const ChangeKeys&, const Config&) {
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
		const Config& cfg = config.current();

		tl::TextStyle textStyle;
		textStyle.setColor(SK_ColorWHITE); // tinted per state at paint time via kModulate
		textStyle.setFontSize(static_cast<SkScalar>(cfg.fontSize));
		if (!cfg.fontFamily.empty()) {
			textStyle.setFontFamilies({SkString(cfg.fontFamily.c_str())});
		}
		textStyle.setFontStyle(SkFontStyle::Normal());

		tl::ParagraphStyle paraStyle;
		paraStyle.setTextStyle(textStyle);
		paraStyle.setTextAlign(toTextAlign(cfg.align));

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
		const Config& cfg       = config.current();
		const float   wrapWidth = std::max(contentWidth, 1.0f);
		for (RenderLine& line : lines_) {
			const std::string& text = line.interlude ? cfg.interludeText : line.text;
			line.paragraph          = buildParagraph(text);
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

	float Renderer::sampleScroll(double nowMs) const {
		const double duration = config.current().scrollDuration;
		if (duration <= 0.0) {
			return scrollTo_;
		}
		const double elapsed = nowMs - scrollAnimStartMs_;
		if (elapsed >= duration) {
			return scrollTo_;
		}
		const float t = easeInOutCubic(static_cast<float>(elapsed / duration));
		return scrollFrom_ + (scrollTo_ - scrollFrom_) * t;
	}

	void Renderer::render(SkCanvas* canvas) {
		if (canvas == nullptr) {
			return;
		}
		const Config& cfg = config.current();

		// Background always fills, even before a lyric loads.
		canvas->clear(static_cast<SkColor>(cfg.backgroundColor));

		if (viewportW_ <= 0 || viewportH_ <= 0) {
			return;
		}

		// Work in logical pixels; the device-pixel ratio is folded into the canvas transform.
		SkAutoCanvasRestore autoRestore(canvas, true);
		canvas->scale(dpr_, dpr_);
		const float logicalW = static_cast<float>(viewportW_) / dpr_;
		const float logicalH = static_cast<float>(viewportH_) / dpr_;

		const float padX         = std::min(static_cast<float>(cfg.paddingX), logicalW * 0.5f);
		const float contentWidth = std::max(logicalW - 2.0f * padX, 1.0f);
		ensureLayout(contentWidth);

		if (lines_.empty()) {
			return;
		}

		const float gap = static_cast<float>(cfg.lineGap);

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
		const float       anchorY     = logicalH * static_cast<float>(cfg.anchor);
		const float       focusCentre = tops[focus] + lines_[focus].height * 0.5f;
		const float       targetY     = focusCentre - anchorY;

		// Ease the scroll towards the focus line: a focus change restarts the tween from the
		// current animated position, while layout shifts under a stable focus retarget in place.
		const double nowMs = clock_.nowMs();
		if (!scrollInit_) {
			scrollFrom_  = targetY;
			scrollTo_    = targetY;
			scrollFocus_ = static_cast<int>(focus);
			scrollInit_  = true;
		} else if (static_cast<int>(focus) != scrollFocus_) {
			scrollFrom_        = sampleScroll(nowMs);
			scrollTo_          = targetY;
			scrollAnimStartMs_ = nowMs;
			scrollFocus_       = static_cast<int>(focus);
		} else if (nowMs - scrollAnimStartMs_ >= cfg.scrollDuration) {
			// Tween finished; track layout-driven target changes instantly.
			scrollFrom_ = targetY;
			scrollTo_   = targetY;
		} else {
			// Tween still running; adjust its landing point without breaking the easing.
			scrollTo_ = targetY;
		}
		const float scrollY = sampleScroll(nowMs);

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

			SkColor color;
			if (line.interlude) {
				color = static_cast<SkColor>(cfg.interludeColor);
			} else {
				const bool isActive = static_cast<int>(i) == activeIndex_;
				color               = static_cast<SkColor>(isActive ? cfg.activeColor : cfg.normalColor);
			}

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
