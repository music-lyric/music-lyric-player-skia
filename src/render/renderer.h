#ifndef MUSIC_LYRIC_PLAYER_RENDER_RENDERER_H_
#define MUSIC_LYRIC_PLAYER_RENDER_RENDERER_H_

#include <cstddef>
#include <memory>
#include <vector>

#include "include/core/SkRefCnt.h"
#include "render/components/line/base/index.h"
#include "render/config/index.h"
#include "render/core/layout.h"
#include "render/core/scroll.h"
#include "utils/clock/clock.h"

class SkCanvas;
class SkFontMgr;
class SkUnicode;

namespace skia::textlayout {
	class FontCollection;
} // namespace skia::textlayout

namespace lyric {
	class Info;
} // namespace lyric

namespace music_lyric_player::playback {
	class Player;
} // namespace music_lyric_player::playback

namespace music_lyric_player::render {
	/**
	 * Lays lyric line components out vertically and paints each frame onto a caller-supplied `SkCanvas`.
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
		 * (Re)lays out every line for the given content width when the layout is dirty.
		 */
		void ensureLayout(float contentWidth, const common::RenderContext& context);

		playback::Player&                         player_;
		sk_sp<SkFontMgr>                          fontMgr_;
		const Clock&                              clock_;
		sk_sp<::skia::textlayout::FontCollection> fonts_;
		sk_sp<SkUnicode>                          unicode_;

		std::vector<std::unique_ptr<components::line::base::Element>> lines_;
		int                                                           activeIndex_ = -1;

		int   viewportW_ = 0; // physical pixels
		int   viewportH_ = 0; // physical pixels
		float dpr_       = 1.0f;

		bool  layoutDirty_ = true;
		float layoutWidth_ = -1.0f; // content width the paragraphs were wrapped to

		core::LayoutManager layout_;
		core::ScrollManager scroll_;

		std::size_t lyricListener_  = 0;
		std::size_t linesListener_  = 0;
		std::size_t configListener_ = 0;
	};
} // namespace music_lyric_player::render

#endif
