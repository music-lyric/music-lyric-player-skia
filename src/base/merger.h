#ifndef MUSIC_LYRIC_PLAYER_BASE_MERGER_H_
#define MUSIC_LYRIC_PLAYER_BASE_MERGER_H_

#include <vector>

#include "info.pb.h"

namespace music_lyric_player::base {
	/**
	 * Clusters consecutive lines whose ends fall within `mergeWindow` so they deactivate together.
	 * Pure arithmetic over line start/end; an earlier line is only ever extended, never cut short.
	 */
	class Merger {
	public:
		/**
		 * Rebuilds the merged end-time table from the lyric and merge settings.
		 * The referenced info must outlive every later query.
		 */
		void build(const ::lyric::Info& info, double mergeWindow, int mergeLimit);

		/**
		 * Returns the merged deactivation time of a line, falling back to the raw time when unbuilt.
		 */
		double getMergedTime(int index) const;

	private:
		/**
		 * Raw deactivation time: the later of a line's own end and the next line's start.
		 * The final line never deactivates; out-of-range indices resolve to 0.
		 */
		double getRawTime(int index) const;

		const ::lyric::Info* info_ = nullptr;
		std::vector<double>  mergedEnd_;
	};
} // namespace music_lyric_player::base

#endif
