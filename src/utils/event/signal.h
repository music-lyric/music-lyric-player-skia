#ifndef MUSIC_LYRIC_PLAYER_UTILS_EVENT_SIGNAL_H_
#define MUSIC_LYRIC_PLAYER_UTILS_EVENT_SIGNAL_H_

#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace music_lyric_player::utils {
	template <typename... Args>
	class Signal {
	public:
		using Listener = std::function<void(Args...)>;

		/**
		 * Registers a listener and returns its id for later removal.
		 */
		std::size_t add(Listener listener) {
			const std::size_t id = this->nextId++;
			this->listeners.push_back({id, std::move(listener)});
			return id;
		}

		void remove(std::size_t id) {
			for (auto it = this->listeners.begin(); it != this->listeners.end(); ++it) {
				if (it->id == id) {
					this->listeners.erase(it);
					return;
				}
			}
		}

		/**
		 * Invokes every listener, isolating a throwing one so it never interrupts the rest.
		 */
		void emit(Args... args) const {
			const std::vector<Entry> snapshot = this->listeners;
			for (const Entry& entry : snapshot) {
				try {
					entry.fn(args...);
				} catch (...) {}
			}
		}

		void clear() {
			this->listeners.clear();
		}

	private:
		struct Entry {
			std::size_t id;
			Listener    fn;
		};

		std::vector<Entry> listeners;
		std::size_t        nextId = 1;
	};
} // namespace music_lyric_player::utils

#endif // MUSIC_LYRIC_PLAYER_UTILS_EVENT_SIGNAL_H_
