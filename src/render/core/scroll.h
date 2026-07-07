#ifndef MUSIC_LYRIC_PLAYER_RENDER_CORE_SCROLL_H_
#define MUSIC_LYRIC_PLAYER_RENDER_CORE_SCROLL_H_

#include "render/utils/animation/tween.h"

namespace music_lyric_player::render::core {
	/**
	 * Eases the vertical scroll offset towards the focus (primary active) line with a clock-driven cubic tween.
	 * A focus change restarts the ease from the current position; a stable focus follows layout drift and snaps once settled.
	 */
	class ScrollManager {
	public:
		ScrollManager();

		/**
		 * Drops the tween so the next `update` snaps into place instead of sliding from the previous song.
		 */
		void reset();

		/**
		 * Eases towards `target` for `focus` over `durationMs` and returns the offset sampled at `now`.
		 */
		float update(double now, float target, int focus, double durationMs);

	private:
		animation::Tween<float> tween;
		bool                    initialized = false;
		int                     focus       = -1;
	};
} // namespace music_lyric_player::render::core

#endif // MUSIC_LYRIC_PLAYER_RENDER_CORE_SCROLL_H_
