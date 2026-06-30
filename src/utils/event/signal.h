#ifndef MUSIC_LYRIC_PLAYER_UTILS_EVENT_SIGNAL_H_
#define MUSIC_LYRIC_PLAYER_UTILS_EVENT_SIGNAL_H_

#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace music_lyric_player {
	/**
	 * Minimal strongly-typed event channel: one `Signal` per event.
	 * Replaces the web `Event<EventMap>` keyed emitter.
	 */
	template <typename... Args>
	class Signal {
	public:
		using Listener = std::function<void(Args...)>;

		/**
		 * Registers a listener and returns its id for later removal.
		 */
		std::size_t add(Listener listener) {
			const std::size_t id = nextId_++;
			listeners_.push_back({id, std::move(listener)});
			return id;
		}

		/**
		 * Removes a previously registered listener by id.
		 */
		void remove(std::size_t id) {
			for (auto it = listeners_.begin(); it != listeners_.end(); ++it) {
				if (it->id == id) {
					listeners_.erase(it);
					return;
				}
			}
		}

		/**
		 * Invokes every listener with the given arguments.
		 * A throwing listener is isolated so it never interrupts the rest.
		 */
		void emit(Args... args) const {
			const std::vector<Entry> snapshot = listeners_;
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
			listeners_.clear();
		}

	private:
		struct Entry {
			std::size_t id;
			Listener    fn;
		};

		std::vector<Entry> listeners_;
		std::size_t        nextId_ = 1;
	};
} // namespace music_lyric_player

#endif
