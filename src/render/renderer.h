#ifndef MUSIC_LYRIC_PLAYER_RENDER_RENDERER_H_
#define MUSIC_LYRIC_PLAYER_RENDER_RENDERER_H_

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "include/core/SkRefCnt.h"
#include "render/config/index.h"
#include "utils/clock/clock.h"

class SkCanvas;
class SkFontMgr;
class SkUnicode;

namespace skia::textlayout {
	class FontCollection;
	class Paragraph;
} // namespace skia::textlayout

namespace lyric {
	class Info;
} // namespace lyric

namespace music_lyric_player::playback {
	class Player;
} // namespace music_lyric_player::playback

namespace music_lyric_player::render {
	/**
	 * Lays lyric lines out vertically with SkParagraph and paints each frame onto a caller-supplied `SkCanvas`.
	 * Owns no window or GPU context; holds the timing engine by reference and subscribes to its events.
	 * Scrolling eases towards the active line with a clock-driven cubic tween.
	 */
	class Renderer {
	public:
		/**
		 * Binds to the timing engine, a system font manager and the shared clock.
		 * The clock is sampled each frame to drive the scroll ease.
		 */
		Renderer(playback::Player& player, sk_sp<SkFontMgr> fontMgr, const Clock& clock);

		/**
		 * Unsubscribes from the timing engine and releases layout resources.
		 */
		~Renderer();

		Renderer(const Renderer&)            = delete;
		Renderer& operator=(const Renderer&) = delete;

		/**
		 * Updates the surface size in physical pixels and its device-pixel ratio.
		 * A width or dpr change re-wraps every line on the next frame.
		 */
		void setViewport(int widthPx, int heightPx, float dpr = 1.0f);

		/**
		 * Paints the current frame onto `canvas`; a no-op when the viewport is empty.
		 */
		void render(SkCanvas* canvas);

		/**
		 * Detaches from the timing engine and drops all lines; safe to call before `player` is destroyed.
		 */
		void dispose();

		config::Manager config;

	private:
		/**
		 * One lyric line's render state: its plain text plus lazily built, width-specific layout.
		 */
		struct RenderLine {
			int                                            index     = -1;
			bool                                           interlude = false;
			std::string                                    text;
			std::unique_ptr<::skia::textlayout::Paragraph> paragraph;
			float                                          height = 0.0f;
		};

		/**
		 * Rebuilds the line list from a freshly loaded lyric.
		 */
		void handleLyricUpdate(const ::lyric::Info& info);

		/**
		 * Records the new focus (primary active) line index used to centre scrolling.
		 */
		void handleLinesUpdate(int firstIndex);

		/**
		 * Marks the layout dirty so styling changes re-wrap on the next frame.
		 */
		void handleConfigUpdate();

		/**
		 * Extracts plain text / interlude flags from the lyric into `lines_`.
		 */
		void rebuildLines(const ::lyric::Info& info);

		/**
		 * (Re)builds each line's paragraph for the given content width when the layout is dirty.
		 */
		void ensureLayout(float contentWidth);

		/**
		 * Builds one white-text paragraph for `text`; the state colour is applied later at paint.
		 */
		std::unique_ptr<::skia::textlayout::Paragraph> buildParagraph(const std::string& text) const;

		/**
		 * Samples the in-flight scroll tween at `nowMs`, easing from `scrollFrom_` to `scrollTo_`.
		 * Returns `scrollTo_` once the tween has run past its duration or animation is disabled.
		 */
		float sampleScroll(double nowMs) const;

		playback::Player&                         player_;
		sk_sp<SkFontMgr>                          fontMgr_;
		const Clock&                              clock_;
		sk_sp<::skia::textlayout::FontCollection> fonts_;
		sk_sp<SkUnicode>                          unicode_;

		std::vector<RenderLine> lines_;
		int                     activeIndex_ = -1;

		int   viewportW_ = 0; // physical pixels
		int   viewportH_ = 0; // physical pixels
		float dpr_       = 1.0f;

		bool  layoutDirty_ = true;
		float layoutWidth_ = -1.0f; // content width the paragraphs were wrapped to

		// Scroll ease state: the tween runs from `scrollFrom_` to `scrollTo_` starting at `scrollAnimStartMs_`.
		// `scrollFocus_` is the focus line the current tween targets; a new focus restarts the tween.
		bool   scrollInit_       = false;
		int    scrollFocus_      = -1;
		float  scrollFrom_       = 0.0f;
		float  scrollTo_         = 0.0f;
		double scrollAnimStartMs_ = 0.0;

		std::size_t lyricListener_  = 0;
		std::size_t linesListener_  = 0;
		std::size_t configListener_ = 0;
	};
} // namespace music_lyric_player::render

#endif
