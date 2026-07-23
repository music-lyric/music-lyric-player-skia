#ifndef MUSIC_LYRIC_PLAYER_UTILS_CONFIG_MANAGER_H_
#define MUSIC_LYRIC_PLAYER_UTILS_CONFIG_MANAGER_H_

#include "utils/config/access.h"
#include "utils/event/signal.h"

namespace music_lyric_player::utils::config {
	template <typename Config>
	class Manager {
	public:
		/**
		 * Resolves the initial config from the empty accumulated overrides.
		 */
		Manager() {
			resolve(this->currentConfig, this->accumulated, Access{});
		}

		/**
		 * Folds a sparse config (only its explicitly-set leaves) into the accumulated overrides, then re-resolves.
		 */
		void merge(const Config& incoming) {
			const Config prev = this->currentConfig;
			overlay(this->accumulated, incoming, Access{});
			reresolve(prev);
		}

		/**
		 * Edits the resolved config through `fn`, records the changed leaves into the accumulated overrides, then re-resolves.
		 */
		template <typename Fn>
		void modify(Fn&& fn) {
			const Config prev = this->currentConfig;
			Config       work = prev;
			fn(work);
			capture(this->accumulated, prev, work, Access{});
			reresolve(prev);
		}

		/**
		 * Clears the accumulated overrides back to defaults, re-resolves, then emits `onReset`.
		 */
		void reset() {
			const Config prev = this->currentConfig;
			this->accumulated = Config{};
			reresolve(prev);
			this->onReset.emit(this->currentConfig);
		}

		const Config& current() const {
			return this->currentConfig;
		}

		Signal<const Config&, const Config&> onUpdate;
		Signal<const Config&>                onReset;

	private:
		/**
		 * Re-resolves the accumulated overrides and, when the resolved config actually changed, emits `onUpdate` with a delta config whose set leaves mark what changed.
		 */
		void reresolve(const Config& prev) {
			resolve(this->currentConfig, this->accumulated, Access{});
			if (!(prev == this->currentConfig)) {
				Config delta;
				capture(delta, prev, this->currentConfig, Access{});
				this->onUpdate.emit(delta, this->currentConfig);
			}
		}

		Config accumulated;
		Config currentConfig;
	};
} // namespace music_lyric_player::utils::config

#endif // MUSIC_LYRIC_PLAYER_UTILS_CONFIG_MANAGER_H_
