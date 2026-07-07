#include "render/renderer.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkFontMgr.h"
#include "info.pb.h"
#include "modules/skparagraph/include/FontCollection.h"
#include "modules/skunicode/include/SkUnicode.h"
#include "modules/skunicode/include/SkUnicode_icu.h"
#include "playback/player.h"
#include "render/common/context.h"
#include "render/components/line/base/index.h"
#include "render/utils/color/parse.h"
#include "render/utils/length.h"

namespace tl = ::skia::textlayout;

namespace music_lyric_player::render {
	namespace {
		// Numeric fallbacks mirroring the string length defaults in the config schema,
		// used when a config value fails to parse.
		constexpr double kDefaultPaddingX = 48.0; // container.paddingX ("48px")
		constexpr double kDefaultGap      = 16.0; // layout.gap         ("16px")

		// Colour fallback mirroring the config default, used when the string fails to parse.
		constexpr SkColor kDefaultBackgroundColor = 0xff101014; // container.backgroundColor ("#101014")
	} // namespace

	Renderer::Renderer(playback::Player& player, sk_sp<SkFontMgr> fontMgr, const Clock& clock)
	    : player(player),
	      fontMgr(std::move(fontMgr)),
	      clock(clock) {
		// Resolve families from the system font manager, with per-glyph fallback so mixed scripts do not render as tofu.
		this->fonts = sk_make_sp<tl::FontCollection>();
		this->fonts->setDefaultFontManager(this->fontMgr);
		this->fonts->enableFontFallback();

		// Unicode backend drives SkParagraph's word / grapheme / line-break boundaries.
		this->unicode = SkUnicodes::ICU::Make();

		this->lyricListener  = this->player.onLyricUpdate.add([this](const ::lyric::Info& info) {
			handleLyricUpdate(info);
		});
		this->linesListener  = this->player.onLinesUpdate.add([this](const std::vector<int>&, int firstIndex, bool) {
			handleLinesUpdate(firstIndex);
		});
		this->configListener = this->config.onUpdate.add([this](const config::ChangeKeys&, const config::Root&) {
			handleConfigUpdate();
		});

		// Adopt any lyric already loaded before the renderer attached.
		if (this->player.currentInfo().lines_size() > 0) {
			rebuildLines(this->player.currentInfo());
			this->activeIndex = this->player.currentActive();
		}
	}

	Renderer::~Renderer() {
		dispose();
	}

	void Renderer::dispose() {
		if (this->lyricListener != 0) {
			this->player.onLyricUpdate.remove(this->lyricListener);
			this->lyricListener = 0;
		}
		if (this->linesListener != 0) {
			this->player.onLinesUpdate.remove(this->linesListener);
			this->linesListener = 0;
		}
		if (this->configListener != 0) {
			this->config.onUpdate.remove(this->configListener);
			this->configListener = 0;
		}
		this->lines.clear();
		this->activeIndex = -1;
	}

	void Renderer::setViewport(int widthPx, int heightPx, float dpr) {
		dpr = (std::isfinite(dpr) && dpr > 0.0f) ? dpr : 1.0f;
		if (widthPx == this->viewportW && heightPx == this->viewportH && dpr == this->dpr) {
			return;
		}
		// A width or dpr change re-wraps every line; a height change only shifts scrolling.
		if (widthPx != this->viewportW || dpr != this->dpr) {
			this->lines.invalidateLayout();
		}
		this->viewportW = std::max(widthPx, 0);
		this->viewportH = std::max(heightPx, 0);
		this->dpr       = dpr;
	}

	void Renderer::handleLyricUpdate(const ::lyric::Info& info) {
		rebuildLines(info);
		this->activeIndex = -1;
	}

	void Renderer::handleLinesUpdate(int firstIndex) {
		this->activeIndex = firstIndex;
	}

	void Renderer::handleConfigUpdate() {
		this->lines.invalidateLayout();
	}

	void Renderer::rebuildLines(const ::lyric::Info& info) {
		this->lines.rebuild(info);
		// Drop the scroll tween so a freshly loaded lyric snaps into place instead of sliding from the old song.
		this->scroll.reset();
	}

	void Renderer::render(SkCanvas* canvas) {
		if (canvas == nullptr) {
			return;
		}
		const config::Root&         cfg = this->config.current();
		const common::RenderContext context{cfg, this->fonts, this->unicode};

		// Background always fills, even before a lyric loads.
		canvas->clear(utils::color::resolve(cfg.container.backgroundColor, kDefaultBackgroundColor));

		if (this->viewportW <= 0 || this->viewportH <= 0) {
			return;
		}

		// Work in logical pixels; the device-pixel ratio is folded into the canvas transform.
		SkAutoCanvasRestore autoRestore(canvas, true);
		canvas->scale(this->dpr, this->dpr);
		const float logicalW = static_cast<float>(this->viewportW) / this->dpr;
		const float logicalH = static_cast<float>(this->viewportH) / this->dpr;

		const float padX         = std::min(static_cast<float>(resolveLength(cfg.container.paddingX, kDefaultPaddingX, logicalW)), logicalW * 0.5f);
		const float contentWidth = std::max(logicalW - 2.0f * padX, 1.0f);
		this->lines.ensureLayout(contentWidth, context);

		if (this->lines.empty()) {
			return;
		}

		const float gap = static_cast<float>(resolveLength(cfg.layout.gap, kDefaultGap, logicalH));
		this->layout.update(this->lines.all(), gap);

		// Centre the focus (primary active) line on the anchor, then ease the scroll towards it.
		const std::size_t focus   = (this->activeIndex >= 0 && this->activeIndex < static_cast<int>(this->lines.size()))
			? static_cast<std::size_t>(this->activeIndex)
			: 0;
		const float       anchorY = logicalH * static_cast<float>(cfg.scroll.anchor);
		const float       targetY = this->layout.top(focus) + this->lines.at(focus).height() * 0.5f - anchorY;

		const double now = this->clock.now();
		this->scroll.update(now, targetY, static_cast<int>(focus), this->lines.size(), cfg.scroll.animation);

		for (std::size_t i = 0; i < this->lines.size(); ++i) {
			const components::line::base::Element& line = this->lines.at(i);
			const float                            y    = this->layout.top(i) - this->scroll.offsetAt(i, now);
			// Cull lines fully outside the viewport.
			if (y + line.height() < 0.0f || y > logicalH) {
				continue;
			}
			const bool active = static_cast<int>(i) == this->activeIndex;
			line.paint(canvas, padX, y, now, active, context);
		}
	}
} // namespace music_lyric_player::render
