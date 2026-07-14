#ifndef MUSIC_LYRIC_PLAYER_RENDERER_H_
#define MUSIC_LYRIC_PLAYER_RENDERER_H_

#include <utility>

#include "abi.h"
#include "player.h"

namespace music_lyric_player {
	/**
	 * Owning, move-only C++ facade over the renderer C ABI; obtain one from `createRenderer`.
	 * Borrows the player it was created from, which must outlive the renderer.
	 */
	class Renderer {
	public:
		explicit Renderer(music_lyric_player_renderer_handle* handle) noexcept
			: handle(handle) {}

		Renderer(const Renderer&)            = delete;
		Renderer& operator=(const Renderer&) = delete;

		Renderer(Renderer&& other) noexcept
			: handle(std::exchange(other.handle, nullptr)) {}

		Renderer& operator=(Renderer&& other) noexcept {
			if (this != &other) {
				this->reset();
				this->handle = std::exchange(other.handle, nullptr);
			}
			return *this;
		}

		~Renderer() {
			this->reset();
		}

		void render() {
			music_lyric_player_renderer_render(this->handle);
		}

		void resize() {
			music_lyric_player_renderer_resize(this->handle);
		}

		explicit operator bool() const noexcept {
			return this->handle != nullptr;
		}

		music_lyric_player_renderer_handle* get() const noexcept {
			return this->handle;
		}

		music_lyric_player_renderer_handle* release() noexcept {
			return std::exchange(this->handle, nullptr);
		}

	private:
		void reset() noexcept {
			if (this->handle != nullptr) {
				music_lyric_player_renderer_destroy(this->handle);
				this->handle = nullptr;
			}
		}

		music_lyric_player_renderer_handle* handle;
	};

	/**
	 * Creates a renderer painting `player` into the native window `window`, empty (`!renderer`) on failure.
	 * The renderer borrows `player`, which must outlive it.
	 */
	inline Renderer createRenderer(Player& player, void* window) {
		return Renderer(music_lyric_player_renderer_create(player.get(), window));
	}
} // namespace music_lyric_player

#endif // MUSIC_LYRIC_PLAYER_RENDERER_H_
