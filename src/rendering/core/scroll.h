#ifndef MUSIC_LYRIC_PLAYER_RENDERING_CORE_SCROLL_H_
#define MUSIC_LYRIC_PLAYER_RENDERING_CORE_SCROLL_H_

#include <cstddef>
#include <vector>

#include "rendering/core/transition.h"
#include "rendering/utils/animation/tween.h"

namespace music_lyric_player::rendering::config::scroll {
	struct AnimationConfig;
} // namespace music_lyric_player::rendering::config::scroll

namespace music_lyric_player::rendering::core {
	/**
	 * Eases each line's vertical scroll offset towards the active line with a clock-driven tween per line.
	 * Every line targets the same offset but starts after its own cascade delay, reproducing the web scroll modes.
	 * A focus change restarts the eases with fresh delays; a stable focus follows layout drift and settles.
	 */
	class ScrollManager {
	public:
		/**
		 * Drops all per-line tweens so the next `update` snaps into place instead of sliding from the previous song.
		 */
		void reset();

		/**
		 * Retargets the per-line offset tweens towards `target` for a `lineCount`-line lyric focused on `focus`.
		 * The `anim` config selects the cascade mode, per-line duration, easing and delay distribution.
		 */
		void update(double now, float target, int focus, ::std::size_t lineCount, const config::scroll::AnimationConfig& anim);

		/**
		 * The eased scroll offset of line `index` sampled at `now`; valid for `index` below the line count after `update`.
		 */
		float offsetAt(::std::size_t index, double now) const {
			return this->offsets[index].sample(now);
		}

	private:
		::std::vector<animation::Tween<float>> offsets;
		bool                                   initialized = false;
		int                                    focus       = -1;
		float                                  lastTarget  = 0.0f; // target the tweens were last pointed at
	};
} // namespace music_lyric_player::rendering::core

#endif // MUSIC_LYRIC_PLAYER_RENDERING_CORE_SCROLL_H_
