#ifndef MUSIC_LYRIC_PLAYER_RENDER_RENDERER_H_
#define MUSIC_LYRIC_PLAYER_RENDER_RENDERER_H_

#include <cstddef>

#include "include/core/SkRefCnt.h"
#include "render/config/index.h"
#include "render/core/effect.h"
#include "render/core/layout.h"
#include "render/core/line.h"
#include "render/core/scroll.h"
#include "utils/clock/clock.h"

class SkCanvas;
class SkFontMgr;
class SkUnicode;

namespace skia::textlayout {
	class FontCollection;
} // namespace skia::textlayout

namespace lyric::runtime {
	class Info;
} // namespace lyric::runtime

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
		void handleLyricUpdate(const ::lyric::runtime::Info& info);

		/**
		 * Records the new focus (primary active) line index used to centre scrolling.
		 */
		void handleLinesUpdate(int firstIndex);

		/**
		 * Marks the layout dirty so styling changes re-wrap on the next frame.
		 */
		void handleConfigUpdate();

		/**
		 * Rebuilds the line list from a freshly loaded lyric and resets scrolling.
		 */
		void rebuildLines(const ::lyric::runtime::Info& info);

		playback::Player&                         player;
		sk_sp<SkFontMgr>                          fontMgr;
		const Clock&                              clock;
		sk_sp<::skia::textlayout::FontCollection> fonts;
		sk_sp<SkUnicode>                          unicode;

		int activeIndex = -1;

		int   viewportW = 0; // physical pixels
		int   viewportH = 0; // physical pixels
		float dpr       = 1.0f;

		core::LineManager   lines;
		core::LayoutManager layout;
		core::ScrollManager scroll;
		core::EffectManager effect;

		std::size_t lyricListener  = 0;
		std::size_t linesListener  = 0;
		std::size_t configListener = 0;
	};
} // namespace music_lyric_player::render

#endif
