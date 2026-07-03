#ifndef MUSIC_LYRIC_PLAYER_PLAYBACK_OFFSET_H_
#define MUSIC_LYRIC_PLAYER_PLAYBACK_OFFSET_H_

#include "info.pb.h"

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

		/**
		 * Clears the temporary offset back to 0.
		 */
		void resetTemp();

		/**
		 * Re-derives the meta offset from the lyric, or clears it when `useMeta` is false.
		 */
		void refreshFromMeta(const ::lyric::Info& info, bool useMeta);

		/**
		 * Combines the given global offset with meta and temp; non-finite results clamp to 0.
		 */
		double resolve(double global) const;

	private:
		double temp_ = 0.0;
		double meta_ = 0.0;
	};
} // namespace music_lyric_player::playback

#endif
