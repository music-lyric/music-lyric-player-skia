#ifndef MUSIC_LYRIC_PLAYER_BASE_CONFIG_H_
#define MUSIC_LYRIC_PLAYER_BASE_CONFIG_H_

#include <optional>
#include <set>
#include <string>

#include "utils/signal.h"

namespace music_lyric_player {
	/**
	 * Set of changed dot-path keys emitted on a config update.
	 */
	using ChangeKeys = std::set<std::string>;
} // namespace music_lyric_player

namespace music_lyric_player::base {
	/**
	 * The global layer of the three-layer offset (`global + meta + temp`).
	 */
	struct OffsetConfig {
		double global                 = 0.0;
		bool   useMeta                = true;
		bool   resetTempOnLyricChange = true;
	};

	/**
	 * Timing-engine configuration: offset, merge window and active bridging.
	 */
	struct Config {
		bool         bridgeActive = true;
		double       mergeWindow  = 300.0;
		int          mergeLimit   = 3;
		OffsetConfig offset;
	};

	/**
	 * Sparse patch: only the set fields are applied on `update`.
	 */
	struct ConfigPatch {
		std::optional<bool>   bridgeActive;
		std::optional<double> mergeWindow;
		std::optional<int>    mergeLimit;

		struct OffsetPatch {
			std::optional<double> global;
			std::optional<bool>   useMeta;
			std::optional<bool>   resetTempOnLyricChange;
		} offset;
	};

	/**
	 * Holds the current config and emits the set of changed dot-paths on change.
	 * Mirrors the web ConfigManager's clone-apply-diff-emit, minus the dynamic layer.
	 */
	class ConfigManager {
	public:
		/**
		 * Applies a sparse patch, then emits `onUpdate` only when something changed.
		 */
		void update(const ConfigPatch& patch);

		/**
		 * Restores defaults and emits `onReset`.
		 */
		void reset();

		/**
		 * Returns the current resolved config.
		 */
		const Config& current() const {
			return current_;
		}

		Signal<const ChangeKeys&, const Config&> onUpdate;
		Signal<const Config&>                    onReset;

	private:
		Config current_;
	};
} // namespace music_lyric_player::base

#endif
