#include "abi.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <memory>
#include <string_view>
#include <type_traits>
#include <vector>

#include "include/core/SkFontMgr.h"
#include "backend/font/font.h"
#include "backend/gpu/surface.h"
#include "playback/player.h"
#include "rendering/config/config.gen.glaze.h"
#include "rendering/config/index.h"
#include "rendering/renderer.h"
#include "music_lyric_model.h"

namespace {
	/**
	 * Logs a C-ABI boundary failure to stderr; exceptions must never cross into the caller.
	 */
	void reportAbiException(const char* what) noexcept {
		std::fprintf(stderr, "[music-lyric-player] c-abi call failed: %s\n", what);
	}

	/**
	 * Runs `fn` and returns its result, swallowing any C++ exception as `fallback`.
	 */
	template <typename Fn>
	std::invoke_result_t<Fn&> guard(Fn&& fn, std::invoke_result_t<Fn&> fallback) noexcept {
		try {
			return fn();
		} catch (const std::exception& error) {
			reportAbiException(error.what());
			return fallback;
		} catch (...) {
			reportAbiException("unknown exception");
			return fallback;
		}
	}

	/**
	 * Runs `fn` for a void-returning entry point, swallowing any C++ exception.
	 */
	template <typename Fn>
	void guardVoid(Fn&& fn) noexcept {
		try {
			fn();
		} catch (const std::exception& error) {
			reportAbiException(error.what());
		} catch (...) {
			reportAbiException("unknown exception");
		}
	}
} // namespace

/**
 * The headless timing engine behind a playback C-ABI handle.
 */
struct music_lyric_player_handle {
	music_lyric_player::playback::Player player;
};

/**
 * The GPU surface and renderer behind a renderer C-ABI handle; the renderer borrows an external player.
 */
struct music_lyric_player_renderer_handle {
	std::unique_ptr<music_lyric_player::backend::gpu::Surface> surface;
	music_lyric_player::rendering::Renderer                       renderer;

	music_lyric_player_renderer_handle(music_lyric_player::playback::Player& player, void* window)
		: surface(music_lyric_player::backend::gpu::createWindowSurface({window})),
		  renderer(player, music_lyric_player::backend::font::createFontMgr(), player.clock()) {}
};

music_lyric_player_handle* music_lyric_player_create(void) {
	return guard([] { return new music_lyric_player_handle(); }, nullptr);
}

void music_lyric_player_destroy(music_lyric_player_handle* player) {
	guardVoid([&] { delete player; });
}

void music_lyric_player_update_lyric(music_lyric_player_handle* player, const uint8_t* data, size_t size) {
	guardVoid([&] {
		const music_lyric_model::parsed::Info info = music_lyric_model::parsed::decodeParsedInfo(
		    std::vector<std::uint8_t>(data, data + size));
		player->player.updateLyric(info);
	});
}

void music_lyric_player_play(music_lyric_player_handle* player) {
	guardVoid([&] { player->player.play(); });
}

void music_lyric_player_play_from(music_lyric_player_handle* player, double seek_ms) {
	guardVoid([&] { player->player.play(seek_ms); });
}

void music_lyric_player_pause(music_lyric_player_handle* player) {
	guardVoid([&] { player->player.pause(); });
}

void music_lyric_player_tick(music_lyric_player_handle* player) {
	guardVoid([&] { player->player.tick(); });
}

void music_lyric_player_update_temp_offset(music_lyric_player_handle* player, double value) {
	guardVoid([&] { player->player.updateTempOffset(value); });
}

int music_lyric_player_current_playing(const music_lyric_player_handle* player) {
	return guard([&] { return player->player.currentPlaying() ? 1 : 0; }, 0);
}

double music_lyric_player_current_time(const music_lyric_player_handle* player) {
	return guard([&] { return player->player.currentTime(); }, 0.0);
}

double music_lyric_player_current_offset(const music_lyric_player_handle* player) {
	return guard([&] { return player->player.currentOffset(); }, 0.0);
}

int music_lyric_player_current_active(const music_lyric_player_handle* player) {
	return guard([&] { return player->player.currentActive(); }, -1);
}

size_t music_lyric_player_current_index(const music_lyric_player_handle* player, int* out, size_t capacity) {
	return guard([&]() -> size_t {
		const std::vector<int> indices = player->player.currentIndex();
		if (out != nullptr) {
			std::copy_n(indices.begin(), std::min(capacity, indices.size()), out);
		}
		return indices.size();
	}, size_t{0});
}

double music_lyric_player_convert_content_time(const music_lyric_player_handle* player, double content_time) {
	return guard([&] { return player->player.convertContentTime(content_time); }, 0.0);
}

void music_lyric_player_on_play(music_lyric_player_handle* player, music_lyric_player_time_callback callback, void* user) {
	guardVoid([&] {
		player->player.onPlay.add([callback, user](double time) { callback(time, user); });
	});
}

void music_lyric_player_on_pause(music_lyric_player_handle* player, music_lyric_player_time_callback callback, void* user) {
	guardVoid([&] {
		player->player.onPause.add([callback, user](double time) { callback(time, user); });
	});
}

void music_lyric_player_on_lines_update(music_lyric_player_handle* player, music_lyric_player_lines_callback callback, void* user) {
	guardVoid([&] {
		player->player.onLinesUpdate.add([callback, user](const std::vector<int>& indices, int active, bool isSeek) {
			callback(indices.data(), indices.size(), active, isSeek ? 1 : 0, user);
		});
	});
}

music_lyric_player_renderer_handle* music_lyric_player_renderer_create(music_lyric_player_handle* player, void* window) {
	return guard([&]() -> music_lyric_player_renderer_handle* {
		auto handle = std::make_unique<music_lyric_player_renderer_handle>(player->player, window);
		if (handle->surface == nullptr) {
			return nullptr;
		}
		return handle.release();
	}, nullptr);
}

void music_lyric_player_renderer_destroy(music_lyric_player_renderer_handle* renderer) {
	guardVoid([&] { delete renderer; });
}

void music_lyric_player_renderer_render(music_lyric_player_renderer_handle* renderer) {
	guardVoid([&] {
		renderer->surface->renderFrame([renderer](SkCanvas* canvas) {
			renderer->renderer.setViewport(renderer->surface->width(),
				renderer->surface->height(),
				renderer->surface->devicePixelRatio());
			renderer->renderer.render(canvas);
		});
	});
}

void music_lyric_player_renderer_resize(music_lyric_player_renderer_handle* renderer) {
	guardVoid([&] { renderer->surface->onResize(); });
}

void music_lyric_player_renderer_update_config_json(music_lyric_player_renderer_handle* renderer, const char* json) {
	guardVoid([&] {
		if (renderer == nullptr || json == nullptr) {
			return;
		}
		// Glaze reflects the sparse patch struct directly: present keys set their optional, absent keys stay unset.
		music_lyric_player::rendering::config::RootPatch patch;
		if (const glz::error_ctx error = glz::read_json(patch, std::string_view(json))) {
			reportAbiException("malformed config json");
			return;
		}
		renderer->renderer.config.merge(patch);
	});
}
