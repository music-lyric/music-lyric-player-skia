#ifndef MUSIC_LYRIC_PLAYER_UTILS_MANAGER_CONFIG_H_
#define MUSIC_LYRIC_PLAYER_UTILS_MANAGER_CONFIG_H_

#include <functional>
#include <set>
#include <string>
#include <string_view>

#include "utils/event/signal.h"

namespace music_lyric_player::utils::config {
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

	template <typename Config, typename Patch>
	class Manager {
	private:
		/**
		 * Emits `onUpdate` with the paths that differ between `prev` and the current config.
		 */
		void emitChangesFrom(const Config& prev) {
			ChangeKeys changed;
			diffConfig(prev, current_, std::string{}, changed);
			if (!changed.empty()) {
				onUpdate.emit(changed, current_);
			}
		}

		Config current_;

	public:
		/**
		 * Applies a sparse patch on top of the current config; emits `onUpdate` if anything changed.
		 */
		void merge(const Patch& patch) {
			const Config prev = current_;
			applyConfigPatch(current_, patch);
			emitChangesFrom(prev);
		}

		/**
		 * Edits the current config in place through `fn`; emits `onUpdate` if anything changed.
		 */
		template <typename Fn>
		void modify(Fn&& fn) {
			const Config prev = current_;
			fn(current_);
			emitChangesFrom(prev);
		}

		/**
		 * Resets every field to its default; emits `onUpdate` with the changes, then `onReset`.
		 */
		void reset() {
			const Config prev = current_;
			current_          = Config{};
			emitChangesFrom(prev);
			onReset.emit(current_);
		}

		/**
		 * Returns the current config.
		 */
		const Config& current() const {
			return current_;
		}

		Signal<const ChangeKeys&, const Config&> onUpdate;
		Signal<const Config&>                    onReset;
	};
} // namespace music_lyric_player::utils::config

#endif
