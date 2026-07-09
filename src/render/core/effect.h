#ifndef MUSIC_LYRIC_PLAYER_RENDER_CORE_EFFECT_H_
#define MUSIC_LYRIC_PLAYER_RENDER_CORE_EFFECT_H_

#include <cstddef>
#include <vector>

#include "render/utils/animation/tween.h"

namespace music_lyric_player::render::config::effect {
	struct Root;
} // namespace music_lyric_player::render::config::effect

namespace music_lyric_player::render::config::scroll {
	struct AnimationConfig;
} // namespace music_lyric_player::render::config::scroll

namespace music_lyric_player::render::core {
	/**
	 * Eases each line's focus effect — scale toward `1` and blur toward `0` as it nears the active line — with a clock-driven tween per line.
	 * Intensity follows a gaussian of the line's distance from the active line; a focus change cascades the eases with the same per-line delay the scroll uses.
	 */
	class EffectManager {
	public:
		/**
		 * Drops all per-line tweens so the next `update` snaps into place instead of easing from the previous song.
		 */
		void reset();

		/**
		 * Retargets each line's scale and blur tween towards its distance-from-`focus` intensity for a `lineCount`-line lyric.
		 * `effect` supplies the scale / blur ranges; `anim` the cascade timing shared with the scroll.
		 */
		void update(double now, int focus, ::std::size_t lineCount, const config::effect::Root& effect, const config::scroll::AnimationConfig& anim);

		/**
		 * The eased scale of line `index` sampled at `now` (`1` = natural size); valid for `index` below the line count after `update`.
		 */
		float scaleAt(::std::size_t index, double now) const {
			return this->scales[index].sample(now);
		}

		/**
		 * The eased blur radius of line `index` sampled at `now`, in logical pixels (`0` = sharp); valid after `update`.
		 */
		float blurAt(::std::size_t index, double now) const {
			return this->blurs[index].sample(now);
		}

	private:
		/**
		 * The gaussian weight (`1` at the focus, decaying to `0` far away) of a line `offset` lines from the active one.
		 */
		static double gaussian(int offset);

		/**
		 * The target scale of a line `offset` lines from the active one under `effect` (`1` when disabled or active).
		 */
		static float targetScale(const config::effect::Root& effect, int offset);

		/**
		 * The target blur radius of a line `offset` lines from the active one under `effect` (`0` when disabled or active).
		 */
		static float targetBlur(const config::effect::Root& effect, int offset);

		::std::vector<animation::Tween<float>> scales;
		::std::vector<animation::Tween<float>> blurs;
		bool                                   initialized = false;
		int                                    focus       = -1;
	};
} // namespace music_lyric_player::render::core

#endif // MUSIC_LYRIC_PLAYER_RENDER_CORE_EFFECT_H_
