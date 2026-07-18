#ifndef MUSIC_LYRIC_PLAYER_PLAYBACK_OFFSET_H_
#define MUSIC_LYRIC_PLAYER_PLAYBACK_OFFSET_H_

#include "music_lyric_model.h"

namespace music_lyric_player::playback {
	/**
	 * Three-layer lyric offset `global + meta + temp` (global is supplied at resolve time).
	 */
	class Offset {
	public:
		/**
		 * Sets the user's temporary offset; non-finite values clamp to 0.
		 */
		void setTemp(double value);

		void resetTemp();

		/**
		 * Re-derives the meta offset from the lyric, or clears it when `useMeta` is false.
		 */
		void refreshFromMeta(const music_lyric_model::parsed::Info& info, bool useMeta);

		/**
		 * Combines the given global offset with meta and temp; non-finite results clamp to 0.
		 */
		double resolve(double global) const;

	private:
		double temp = 0.0;
		double meta = 0.0;
	};
} // namespace music_lyric_player::playback

#endif
