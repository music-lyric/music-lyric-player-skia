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

		explicit Player(music_lyric_player_handle* handle) noexcept
			: handle(handle) {}

		Player(const Player&)            = delete;
		Player& operator=(const Player&) = delete;

		Player(Player&& other) noexcept
			: handle(std::exchange(other.handle, nullptr)), hooks(std::move(other.hooks)) {}

		Player& operator=(Player&& other) noexcept {
			if (this != &other) {
				this->reset();
				this->handle = std::exchange(other.handle, nullptr);
				this->hooks  = std::move(other.hooks);
			}
			return *this;
		}

		~Player() {
			this->reset();
		}

		void updateLyric(const uint8_t* data, std::size_t size) {
			music_lyric_player_update_lyric(this->handle, data, size);
		}

		void play() {
			music_lyric_player_play(this->handle);
		}

		void play(double seekMs) {
			music_lyric_player_play_from(this->handle, seekMs);
		}

		void pause() {
			music_lyric_player_pause(this->handle);
		}

		void tick() {
			music_lyric_player_tick(this->handle);
		}

		void updateTempOffset(double value) {
			music_lyric_player_update_temp_offset(this->handle, value);
		}

		bool currentPlaying() const {
			return music_lyric_player_current_playing(this->handle) != 0;
		}

		double currentTime() const {
			return music_lyric_player_current_time(this->handle);
		}

		double currentOffset() const {
			return music_lyric_player_current_offset(this->handle);
		}

		int currentActive() const {
			return music_lyric_player_current_active(this->handle);
		}

		std::vector<int> currentIndex() const {
			const std::size_t count = music_lyric_player_current_index(this->handle, nullptr, 0);
			std::vector<int>  indices(count);
			if (count != 0) {
				music_lyric_player_current_index(this->handle, indices.data(), count);
			}
			return indices;
		}

		double convertContentTime(double contentTime) const {
			return music_lyric_player_convert_content_time(this->handle, contentTime);
		}

		void onPlay(TimeHook callback) {
			auto holder = std::make_shared<TimeHook>(std::move(callback));
			music_lyric_player_on_play(this->handle, &Player::dispatchTime, holder.get());
			this->hooks.push_back(std::move(holder));
		}

		void onPause(TimeHook callback) {
			auto holder = std::make_shared<TimeHook>(std::move(callback));
			music_lyric_player_on_pause(this->handle, &Player::dispatchTime, holder.get());
			this->hooks.push_back(std::move(holder));
		}

		void onLinesUpdate(LinesHook callback) {
			auto holder = std::make_shared<LinesHook>(std::move(callback));
			music_lyric_player_on_lines_update(this->handle, &Player::dispatchLines, holder.get());
			this->hooks.push_back(std::move(holder));
		}

		explicit operator bool() const noexcept {
			return this->handle != nullptr;
		}

		music_lyric_player_handle* get() const noexcept {
			return this->handle;
		}

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
