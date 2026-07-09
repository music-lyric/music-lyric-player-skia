#ifndef MUSIC_LYRIC_PLAYER_UTILS_MANAGER_CONFIG_H_
#define MUSIC_LYRIC_PLAYER_UTILS_MANAGER_CONFIG_H_

#include "utils/event/signal.h"

namespace music_lyric_player::utils::config {
	template <typename Config, typename Patch, typename Change>
	class Manager {
	private:
		/**
		 * Diffs `prev` against the current config and emits `onUpdate` when anything changed.
		 */
		void emitChangesFrom(const Config& prev) {
			const Change changes = diff(prev, this->currentConfig);
			if (changes.any) {
				this->onUpdate.emit(changes, this->currentConfig);
			}
		}

		Config currentConfig;

	public:
		/**
		 * Applies a sparse patch on top of the current config; emits `onUpdate` if anything changed.
		 */
		void merge(const Patch& patch) {
			const Config prev = this->currentConfig;
			apply(this->currentConfig, patch);
			emitChangesFrom(prev);
		}

		/**
		 * Edits the current config in place through `fn`; emits `onUpdate` if anything changed.
		 */
		template <typename Fn>
		void modify(Fn&& fn) {
			const Config prev = this->currentConfig;
			fn(this->currentConfig);
			emitChangesFrom(prev);
		}

		/**
		 * Resets every field to its default; emits `onUpdate` with the changes, then `onReset`.
		 */
		void reset() {
			const Config prev   = this->currentConfig;
			this->currentConfig = Config{};
			emitChangesFrom(prev);
			this->onReset.emit(this->currentConfig);
		}

		/**
		 * Returns the current config.
		 */
		const Config& current() const {
			return this->currentConfig;
		}

		Signal<const Change&, const Config&> onUpdate;
		Signal<const Config&>                onReset;
	};
} // namespace music_lyric_player::utils::config

#endif
