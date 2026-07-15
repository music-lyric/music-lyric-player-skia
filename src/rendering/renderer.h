#ifndef MUSIC_LYRIC_PLAYER_RENDERING_RENDERER_H_
#define MUSIC_LYRIC_PLAYER_RENDERING_RENDERER_H_

#include <cstddef>
#include <memory>

#include "include/core/SkRefCnt.h"
#include "rendering/config/index.h"
#include "rendering/core/effect.h"
#include "rendering/core/layout.h"
#include "rendering/core/line.h"
#include "rendering/core/scroll.h"
#include "utils/clock/clock.h"

class SkCanvas;
class SkFontMgr;
class SkShaper;
class SkUnicode;

namespace lyric::runtime {
	class Info;
} // namespace lyric::runtime

namespace music_lyric_player::playback {
	class Player;
} // namespace music_lyric_player::playback

namespace music_lyric_player::rendering {
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
		Renderer(playback::Player& player, sk_sp<SkFontMgr> fontMgr, const music_lyric_player::utils::Clock& clock);

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

		playback::Player&                       player;
		sk_sp<SkFontMgr>                        fontMgr;
		const music_lyric_player::utils::Clock& clock;
		sk_sp<SkUnicode>                        unicode;
		std::unique_ptr<SkShaper>               shaper;

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
} // namespace music_lyric_player::rendering

#endif
