#ifndef MUSIC_LYRIC_PLAYER_RENDERER_H_
#define MUSIC_LYRIC_PLAYER_RENDERER_H_

#include <string>
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
		/**
		 * Adopts `handle` and destroys it with the renderer.
		 */
		explicit Renderer(music_lyric_player_renderer_handle* handle) noexcept : handle(handle) {}

		Renderer(const Renderer&)            = delete;
		Renderer& operator=(const Renderer&) = delete;

		/**
		 * Takes over `other`'s handle, leaving it empty.
		 */
		Renderer(Renderer&& other) noexcept : handle(std::exchange(other.handle, nullptr)) {}

		/**
		 * Destroys the current handle, then takes over `other`'s.
		 */
		Renderer& operator=(Renderer&& other) noexcept {
			if (this != &other) {
				this->reset();
				this->handle = std::exchange(other.handle, nullptr);
			}
			return *this;
		}

		/**
		 * Destroys the handle.
		 */
		~Renderer() {
			this->reset();
		}

		/**
		 * Paints one frame into the window.
		 */
		void render() {
			music_lyric_player_renderer_render(this->handle);
		}

		/**
		 * Reports the window's drawing area in physical pixels, at `devicePixelRatio` physical pixels per logical unit.
		 */
		void setViewport(int width, int height, float devicePixelRatio) {
			music_lyric_player_renderer_set_viewport(this->handle, width, height, devicePixelRatio);
		}

		/**
		 * Merges a JSON config patch into the renderer's config; only the fields present in `json` take effect.
		 */
		void updateConfig(const char* json) {
			music_lyric_player_renderer_update_config_json(this->handle, json);
		}

		/**
		 * Merges a JSON config patch into the renderer's config; only the fields present in `json` take effect.
		 */
		void updateConfig(const std::string& json) {
			music_lyric_player_renderer_update_config_json(this->handle, json.c_str());
		}

		/**
		 * Whether the renderer holds a handle.
		 */
		explicit operator bool() const noexcept {
			return this->handle != nullptr;
		}

		/**
		 * The underlying handle, still owned by the renderer.
		 */
		music_lyric_player_renderer_handle* get() const noexcept {
			return this->handle;
		}

		/**
		 * Gives up ownership of the handle, which the caller must then destroy.
		 */
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
