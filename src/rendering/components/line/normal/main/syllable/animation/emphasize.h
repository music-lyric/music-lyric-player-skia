#ifndef MUSIC_LYRIC_PLAYER_RENDERING_COMPONENTS_LINE_NORMAL_MAIN_SYLLABLE_ANIMATION_EMPHASIZE_H_
#define MUSIC_LYRIC_PLAYER_RENDERING_COMPONENTS_LINE_NORMAL_MAIN_SYLLABLE_ANIMATION_EMPHASIZE_H_

#include <cstddef>
#include <string>
#include <vector>

#include "rendering/utils/animation/easing.h"

namespace music_lyric_player::rendering::components::line::normal::main::syllable::animation {
	/**
	 * Per-character emphasize animation of a stressed word: every cell runs a staggered triangle pulse from rest to a peak and back.
	 * The pulse follows playback content time while the line is active, so it stays in sync with the sung word across pause and seek.
	 * A line going inactive winds residual pulses forward on wall-clock time at the configured disable rate, mirroring the web fast-forward rather than a reverse.
	 */
	class Emphasize {
	public:
		/**
		 * One cell's sampled state: a scale factor about the cell center plus a translation applied in the scaled space.
		 */
		struct Transform {
			float scale = 1.0f;
			float dx    = 0.0f;
			float dy    = 0.0f;
		};

		/**
		 * Resolved main sub-effect settings for one sample call; the easing strings reference config storage.
		 */
		struct MainSettings {
			bool               enabled;
			float              scale;
			float              offsetHorizontal;
			float              offsetVertical;
			const std::string& easingRise;
			const std::string& easingFall;
		};

		/**
		 * Caches the word's absolute start and non-negative duration.
		 */
		Emphasize(double start, double duration);

		/**
		 * Samples every cell's transform for this frame and returns the internally cached list, sized to `cellCount`.
		 * `minDuration` stretches the timeline of short syllables and `disableRate` is the wind-down playback rate, both taken raw from config and clamped here.
		 * Cells that never started before the line deactivated hold rest through the wind-down, so a skipped line cannot flash its upcoming words.
		 */
		const std::vector<Transform>& sample(double currentTime, double now, bool active, double minDuration, double disableRate, std::size_t cellCount, const MainSettings& main) const;

	private:
		/**
		 * An easing resolved from a config string, re-resolved only when the string changes.
		 */
		struct CachedEasing {
			/**
			 * Returns the easing for `value`, reusing the cached resolution while the string stays unchanged.
			 */
			const ::music_lyric_player::rendering::animation::Easing& resolve(const std::string& value);

			std::string                                        key;
			::music_lyric_player::rendering::animation::Easing fn;
			bool                                               ready = false;
		};

		double start;
		double duration;

		mutable CachedEasing mainRise;
		mutable CachedEasing mainFall;

		mutable std::vector<Transform> transforms;
		mutable bool                   lastActive = false;
		mutable bool                   exiting    = false;
		mutable double                 lastTime   = 0.0;
		mutable double                 exitBase   = 0.0;
		mutable double                 exitStart  = 0.0;
	};
} // namespace music_lyric_player::rendering::components::line::normal::main::syllable::animation

#endif // MUSIC_LYRIC_PLAYER_RENDERING_COMPONENTS_LINE_NORMAL_MAIN_SYLLABLE_ANIMATION_EMPHASIZE_H_
