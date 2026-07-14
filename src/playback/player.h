#ifndef MUSIC_LYRIC_PLAYER_PLAYBACK_PLAYER_H_
#define MUSIC_LYRIC_PLAYER_PLAYBACK_PLAYER_H_

#include <cstddef>
#include <optional>
#include <vector>

#include "playback/config/index.h"
#include "playback/merger.h"
#include "playback/offset.h"
#include "runtime/info.pb.h"
#include "utils/clock/clock.h"
#include "utils/event/signal.h"

namespace music_lyric_player::playback {
	/**
	 * Headless timing engine: owns the playback clock and tracks active lines.
	 * Holds no rendering concern; the platform injects a `Clock` and drives `tick()`.
	 */
	class Player {
	public:
		/**
		 * Constructs the player with a built-in real-time clock.
		 */
		Player();

		/**
		 * Constructs the player against an injected clock (tests, recording or a shared clock).
		 */
		explicit Player(const utils::Clock& clock);

		/**
		 * Loads a parsed lyric, rejecting versions outside the supported caret range.
		 */
		void updateLyric(const ::lyric::runtime::Info& info);

		/**
		 * Starts or resumes playback, optionally seeking to `seek` first.
		 */
		void play(std::optional<double> seek = std::nullopt);

		/**
		 * Pauses playback, emitting `onPause` only on a real playing-to-paused transition.
		 */
		void pause();

		/**
		 * Advances active-line tracking one step; the platform calls it once per frame.
		 */
		void tick();

		/**
		 * Stops playback, clears listeners and state, and drops the current lyric.
		 */
		void dispose();

		/**
		 * Sets the user's temporary offset (ms) and resyncs immediately.
		 */
		void updateTempOffset(double value);

		/**
		 * Returns the indices of all lines active at `time` (ms) without mutating state.
		 */
		std::vector<int> matchLinesWithTime(double time) const;

		/**
		 * Converts a content time to a playback time by removing the active offset.
		 */
		double convertContentTime(double contentTime) const;

		/**
		 * Whether playback is currently running.
		 */
		bool currentPlaying() const;

		/**
		 * The bridged indices of the currently active lines.
		 */
		std::vector<int> currentIndex() const;

		/**
		 * The index of the primary active line, or -1 when none.
		 */
		int currentActive() const;

		/**
		 * The current lyric info.
		 */
		const ::lyric::runtime::Info& currentInfo() const;

		/**
		 * The current playback time in ms.
		 */
		double currentTime() const;

		/**
		 * The combined effective offset in ms (config global + meta + temp).
		 */
		double currentOffset() const;

		/**
		 * The clock this player reads time from (the renderer shares it).
		 */
		const utils::Clock& clock() const;

		config::Manager config;

		utils::Signal<double>                             onPlay;
		utils::Signal<double>                             onPause;
		utils::Signal<const ::lyric::runtime::Info&>      onLyricUpdate;
		utils::Signal<const std::vector<int>&, int, bool> onLinesUpdate;

	private:
		/**
		 * Current playback time: `seek` while paused, else `seek + (now - start)`.
		 */
		double getCurrentTime() const;

		/**
		 * Playback time shifted by the combined offset; all line matching runs against this.
		 */
		double getEffectiveTime() const;

		void buildMergedLineEnd();

		/**
		 * The primary active index, or -1 when none.
		 */
		int getActiveIndex() const;

		/**
		 * Fills index gaps so the active range reads as one contiguous block.
		 */
		std::vector<int> bridgeActive(const std::vector<int>& index) const;

		void emitLinesUpdate(bool isSeek);

		/**
		 * Full re-scan of active lines on seek / offset / config change.
		 */
		void syncTime(std::optional<double> time = std::nullopt);

		/**
		 * Incremental active-line advance from the current scan cursor.
		 */
		void updateActiveLines(double now);

		/**
		 * Reacts to a config change by re-deriving offset / merge and resyncing.
		 */
		void onConfigUpdate(const config::RootChange& changes);

		const utils::Clock&    clockRef;
		bool                   playing   = false;
		int                    scanIndex = 0;
		std::vector<int>       activeIndex;
		double                 start = 0.0;
		double                 seek  = 0.0;
		::lyric::runtime::Info info;
		Merger                 merger;
		Offset                 offset;
		std::size_t            configListenerId = 0;
	};
} // namespace music_lyric_player::playback

#endif
