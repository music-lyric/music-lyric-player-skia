#ifndef MUSIC_LYRIC_PLAYER_UTILS_EVENT_SIGNAL_H_
#define MUSIC_LYRIC_PLAYER_UTILS_EVENT_SIGNAL_H_

#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace music_lyric_player {
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

		/**
		 * Removes a previously registered listener by id.
		 */
		void remove(std::size_t id) {
			for (auto it = this->listeners.begin(); it != this->listeners.end(); ++it) {
				if (it->id == id) {
					this->listeners.erase(it);
					return;
				}
			}
		}

		/**
		 * Invokes every listener with the given arguments.
		 * A throwing listener is isolated so it never interrupts the rest.
		 */
		void emit(Args... args) const {
			const std::vector<Entry> snapshot = this->listeners;
			for (const Entry& entry : snapshot) {
				try {
					entry.fn(args...);
				} catch (...) {
				}
			}
		}

		/**
		 * Drops all listeners.
		 */
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
} // namespace music_lyric_player

#endif
