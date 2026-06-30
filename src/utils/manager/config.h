#ifndef MUSIC_LYRIC_PLAYER_UTILS_MANAGER_CONFIG_H_
#define MUSIC_LYRIC_PLAYER_UTILS_MANAGER_CONFIG_H_

#include <functional>
#include <initializer_list>
#include <set>
#include <string>
#include <string_view>

#include "utils/event/signal.h"

namespace music_lyric_player {
	/**
	 * Set of changed dot-path keys emitted on a config update.
	 * Transparent comparator so lookups accept `string_view` / generated key constants.
	 */
	using ChangeKeys = std::set<std::string, std::less<>>;

	/**
	 * Whether an exact dot-path key changed.
	 */
	inline bool keyHas(const ChangeKeys& keys, std::string_view path) {
		return keys.count(path) > 0;
	}

	/**
	 * Whether any changed key contains the given substring.
	 */
	inline bool keyContains(const ChangeKeys& keys, std::string_view sub) {
		for (const auto& key : keys) {
			if (key.find(sub) != std::string::npos) {
				return true;
			}
		}
		return false;
	}

	/**
	 * Whether any changed key contains any of the given substrings.
	 */
	inline bool keyContainsAny(const ChangeKeys& keys, std::initializer_list<std::string_view> subs) {
		for (const auto& sub : subs) {
			if (keyContains(keys, sub)) {
				return true;
			}
		}
		return false;
	}

	/**
	 * Generic config store mirroring the web ConfigManager: clone, apply, diff, emit only on change.
	 * `Config` is the fully-resolved struct; `Patch` is its deeply-optional mirror.
	 * `applyConfigPatch` / `diffConfig` are generated per config (see script/generate-config.py) and found by ADL.
	 */
	template <typename Config, typename Patch>
	class ConfigManager {
	public:
		/**
		 * Deep-merges a sparse patch, then emits `onUpdate` only when something changed.
		 */
		void update(const Patch& patch) {
			const Config prev = current_;
			applyConfigPatch(current_, patch);

			ChangeKeys changed;
			diffConfig(prev, current_, std::string{}, changed);
			if (!changed.empty()) {
				onUpdate.emit(changed, current_);
			}
		}

		/**
		 * Restores defaults, emitting the changed paths via `onUpdate` before `onReset`, as the web manager does.
		 */
		void reset() {
			const Config prev = current_;
			current_          = Config{};

			ChangeKeys changed;
			diffConfig(prev, current_, std::string{}, changed);
			if (!changed.empty()) {
				onUpdate.emit(changed, current_);
			}
			onReset.emit(current_);
		}

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
} // namespace music_lyric_player

#endif
