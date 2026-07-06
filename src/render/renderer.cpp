#include "render/renderer.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkFontMgr.h"
#include "info.pb.h"
#include "line/content.h"
#include "modules/skparagraph/include/FontCollection.h"
#include "modules/skunicode/include/SkUnicode.h"
#include "modules/skunicode/include/SkUnicode_icu.h"
#include "playback/player.h"
#include "render/common/context.h"
#include "render/components/line/interlude/index.h"
#include "render/components/line/normal/index.h"
#include "render/utils/animation/easing.h"
#include "render/utils/length.h"

namespace tl = ::skia::textlayout;

namespace music_lyric_player::render {
	namespace {
		// Numeric fallbacks mirroring the string length defaults in the config schema,
		// used when a config value fails to parse.
		constexpr double kDefaultPaddingX = 48.0; // container.paddingX ("48px")
		constexpr double kDefaultGap      = 16.0; // layout.gap         ("16px")
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
			if (::music_lyric_model::isLineInterlude(line)) {
				lines_.push_back(std::make_unique<components::line::interlude::Element>(i));
			} else {
				lines_.push_back(std::make_unique<components::line::normal::Element>(i, ::music_lyric_model::getLineText(line)));
			}
		}
		// Drop the scroll tween so a freshly loaded lyric snaps into place instead of sliding from the old song.
		scrollInit_  = false;
		scrollFocus_ = -1;
		layoutDirty_ = true;
	}

	void Renderer::ensureLayout(float contentWidth, const common::RenderContext& context) {
		if (!layoutDirty_ && contentWidth == layoutWidth_) {
			return;
		}
		for (const std::unique_ptr<components::line::base::Element>& line : lines_) {
			line->layout(contentWidth, context);
		}
		layoutDirty_ = false;
		layoutWidth_ = contentWidth;
	}

	void Renderer::render(SkCanvas* canvas) {
		if (canvas == nullptr) {
			return;
		}
		const config::Root&         cfg = config.current();
		const common::RenderContext context{cfg, fonts_, unicode_};

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
		ensureLayout(contentWidth, context);

		if (lines_.empty()) {
			return;
		}

		const float gap = static_cast<float>(resolveLength(cfg.layout.gap, kDefaultGap, logicalH));

		// Stack each line below the previous plus the gap; no off-screen virtualization yet.
		std::vector<float> tops(lines_.size(), 0.0f);
		float              cursor = 0.0f;
		for (std::size_t i = 0; i < lines_.size(); ++i) {
			tops[i] = cursor;
			cursor += lines_[i]->height() + gap;
		}

		// Centre the focus (primary active) line on the anchor.
		const std::size_t focus       = (activeIndex_ >= 0 && activeIndex_ < static_cast<int>(lines_.size()))
			? static_cast<std::size_t>(activeIndex_)
			: 0;
		const float       anchorY     = logicalH * static_cast<float>(cfg.scroll.anchor);
		const float       focusCentre = tops[focus] + lines_[focus]->height() * 0.5f;
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
			components::line::base::Element& line = *lines_[i];
			const float                     y    = tops[i] - scrollY;
			// Cull lines fully outside the viewport.
			if (y + line.height() < 0.0f || y > logicalH) {
				continue;
			}
			const bool active = static_cast<int>(i) == activeIndex_;
			line.paint(canvas, padX, y, active, context);
		}
	}
} // namespace music_lyric_player::render
