#ifndef MUSIC_LYRIC_PLAYER_PLAYER_H_
#define MUSIC_LYRIC_PLAYER_PLAYER_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "abi.h"

namespace music_lyric_player {
	/**
	 * Owning, move-only C++ facade over the playback C ABI; obtain one from `createPlayer`.
	 * Registered hooks stay alive until the player is destroyed.
	 */
	class Player {
	public:
		using TimeHook  = std::function<void(double)>;
		using LinesHook = std::function<void(const std::vector<int>&, int, bool)>;

		/**
		 * Adopts `handle` and destroys it with the player.
		 */
		explicit Player(music_lyric_player_handle* handle) noexcept : handle(handle) {}

		Player(const Player&)            = delete;
		Player& operator=(const Player&) = delete;

		/**
		 * Takes over `other`'s handle and hooks, leaving it empty.
		 */
		Player(Player&& other) noexcept : handle(std::exchange(other.handle, nullptr)), hooks(std::move(other.hooks)) {}

		/**
		 * Destroys the current handle, then takes over `other`'s.
		 */
		Player& operator=(Player&& other) noexcept {
			if (this != &other) {
				this->reset();
				this->handle = std::exchange(other.handle, nullptr);
				this->hooks  = std::move(other.hooks);
			}
			return *this;
		}

		/**
		 * Destroys the handle and drops every registered hook.
		 */
		~Player() {
			this->reset();
		}

		/**
		 * Replaces the current lyric with `size` bytes in the model's protobuf wire format.
		 */
		void updateLyric(const uint8_t* data, std::size_t size) {
			music_lyric_player_update_lyric(this->handle, data, size);
		}

		/**
		 * Starts or resumes playback at the current position.
		 */
		void play() {
			music_lyric_player_play(this->handle);
		}

		/**
		 * Seeks to `seekMs` and starts playback.
		 */
		void play(double seekMs) {
			music_lyric_player_play_from(this->handle, seekMs);
		}

		/**
		 * Pauses playback.
		 */
		void pause() {
			music_lyric_player_pause(this->handle);
		}

		/**
		 * Advances active-line tracking one step; call once per frame.
		 */
		void tick() {
			music_lyric_player_tick(this->handle);
		}

		/**
		 * Sets the user's temporary offset in milliseconds and resyncs.
		 */
		void updateTempOffset(double value) {
			music_lyric_player_update_temp_offset(this->handle, value);
		}

		/**
		 * Whether playback is currently running.
		 */
		bool currentPlaying() const {
			return music_lyric_player_current_playing(this->handle) != 0;
		}

		/**
		 * The current playback time in milliseconds.
		 */
		double currentTime() const {
			return music_lyric_player_current_time(this->handle);
		}

		/**
		 * The combined effective offset in milliseconds (config global + meta + temp).
		 */
		double currentOffset() const {
			return music_lyric_player_current_offset(this->handle);
		}

		/**
		 * The primary active line index, or -1 when none.
		 */
		int currentActive() const {
			return music_lyric_player_current_active(this->handle);
		}

		/**
		 * The full active-line index set.
		 */
		std::vector<int> currentIndex() const {
			const std::size_t count = music_lyric_player_current_index(this->handle, nullptr, 0);
			std::vector<int>  indices(count);
			if (count != 0) {
				music_lyric_player_current_index(this->handle, indices.data(), count);
			}
			return indices;
		}

		/**
		 * Converts a content time to a playback time by removing the active offset.
		 */
		double convertContentTime(double contentTime) const {
			return music_lyric_player_convert_content_time(this->handle, contentTime);
		}

		/**
		 * Registers a listener fired when playback starts or resumes.
		 */
		void onPlay(TimeHook callback) {
			auto holder = std::make_shared<TimeHook>(std::move(callback));
			music_lyric_player_on_play(this->handle, &Player::dispatchTime, holder.get());
			this->hooks.push_back(std::move(holder));
		}

		/**
		 * Registers a listener fired when playback pauses.
		 */
		void onPause(TimeHook callback) {
			auto holder = std::make_shared<TimeHook>(std::move(callback));
			music_lyric_player_on_pause(this->handle, &Player::dispatchTime, holder.get());
			this->hooks.push_back(std::move(holder));
		}

		/**
		 * Registers a listener fired when the active-line set changes.
		 */
		void onLinesUpdate(LinesHook callback) {
			auto holder = std::make_shared<LinesHook>(std::move(callback));
			music_lyric_player_on_lines_update(this->handle, &Player::dispatchLines, holder.get());
			this->hooks.push_back(std::move(holder));
		}

		/**
		 * Whether the player holds a handle.
		 */
		explicit operator bool() const noexcept {
			return this->handle != nullptr;
		}

		/**
		 * The underlying handle, still owned by the player.
		 */
		music_lyric_player_handle* get() const noexcept {
			return this->handle;
		}

		/**
		 * Gives up ownership of the handle, which the caller must then destroy.
		 */
		music_lyric_player_handle* release() noexcept {
			return std::exchange(this->handle, nullptr);
		}

	private:
		static void dispatchTime(double timeMs, void* user) {
			(*static_cast<TimeHook*>(user))(timeMs);
		}

		static void dispatchLines(const int* indices, std::size_t count, int active, int isSeek, void* user) {
			const std::vector<int> values(indices, indices + count);
			(*static_cast<LinesHook*>(user))(values, active, isSeek != 0);
		}

		void reset() noexcept {
			if (this->handle != nullptr) {
				music_lyric_player_destroy(this->handle);
				this->handle = nullptr;
			}
			this->hooks.clear();
		}

		music_lyric_player_handle* handle;
		// Keeps hook callbacks alive at stable addresses for the C trampolines' user pointers.
		std::vector<std::shared_ptr<void>> hooks;
	};

	/**
	 * Creates a headless player, empty (`!player`) on failure.
	 */
	inline Player createPlayer() {
		return Player(music_lyric_player_create());
	}
} // namespace music_lyric_player

#endif // MUSIC_LYRIC_PLAYER_PLAYER_H_
