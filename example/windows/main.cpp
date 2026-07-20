#include <cstdio>
#include <exception>
#include <string>

#include "include/core/SkFontMgr.h"
#include "backend/font/font.h"
#include "backend/gpu/surface.h"
#include "music_lyric_model.h"
#include "playback/player.h"
#include "rendering/config/index.h"
#include "rendering/renderer.h"
#include "lyric_input.h"
#include "window.h"

int main() {
	example::Window window;
	if (!window.init(1280, 800, "music-lyric-player - skia")) {
		std::fprintf(stderr, "[example] failed to initialise the window\n");
		return 1;
	}

	auto surface = music_lyric_player::backend::gpu::createWindowSurface({window.hwnd()});
	if (surface == nullptr) {
		std::fprintf(stderr, "[example] failed to create the backend surface\n");
		return 1;
	}

	sk_sp<SkFontMgr> fontMgr = music_lyric_player::backend::font::createFontMgr();

	music_lyric_player::playback::Player player;
	music_lyric_player::rendering::Renderer renderer(player, fontMgr, player.clock());

	// DirectWrite cannot resolve a generic "sans-serif" family, so pick a concrete system family covering CJK and latin.
	// The demo also left-aligns the lyrics.
	renderer.config.modify([](music_lyric_player::rendering::config::Root& cfg) {
		cfg.layout.align                                              = music_lyric_player::rendering::config::layout::Align::Left;
		cfg.line.normal.main.syllable.font.size                       = "42px";
		cfg.line.normal.main.syllable.font.family                     = "MiSans";
		cfg.line.normal.main.syllable.style.normal.color              = "rgb(255, 255, 61)";
		cfg.line.normal.main.syllable.style.active.color              = "#ffffff";
		cfg.scroll.animation.mode                                     = music_lyric_player::rendering::config::scroll::Mode::Stagger;
		cfg.scroll.animation.stagger.duration                         = 500;
		cfg.scroll.animation.stagger.easing                           = "ease";
		cfg.scroll.animation.stagger.step                             = 40;
		cfg.scroll.animation.stagger.range                            = 4;
		cfg.line.normal.main.syllable.word.animation.mask.enabled     = true;
		cfg.line.normal.main.syllable.word.animation.floating.enabled = true;
	});

	bool        paused = false;
	std::string lastHex;

	// Loads a parsed lyric, restarts playback from the top and reflects the source in the title bar.
	const auto load = [&](const music_lyric_model::parsed::Info& info, const std::string& source) {
		paused = false;
		player.updateLyric(info);
		player.play(0.0);
		window.setTitle(("music-lyric-player - skia  " + source).c_str());
	};

	// Restore the lyric loaded on a previous run, so externally supplied input persists across launches.
	if (const auto persisted = example::loadPersistedLyric()) {
		try {
			load(music_lyric_model::parsed::decodeParsedInfo(*persisted), "[restored]");
		} catch (const std::exception& error) {
			std::fprintf(stderr, "[example] failed to decode the persisted lyric: %s\n", error.what());
		}
	} else {
		window.setTitle("music-lyric-player - skia  [press L to load a hex lyric]");
	}

	std::printf("[example] controls: L = load hex lyric, R = restart, Space = pause.\n");

	while (!window.shouldClose()) {
		window.pollEvents();

		for (example::InputAction action : window.drainActions()) {
			switch (action) {
				case example::InputAction::Restart:
					paused = false;
					player.play(0.0);
					break;
				case example::InputAction::TogglePause:
					paused = !paused;
					if (paused) {
						player.pause();
					} else {
						player.play();
					}
					break;
				case example::InputAction::LoadHex: {
					const auto entered = example::promptHexLyric(window.hwnd(), lastHex);
					if (!entered) {
						break;
					}
					const auto bytes = example::decodeHex(*entered);
					if (!bytes) {
						example::reportError(window.hwnd(), "The input is not valid hex.");
						break;
					}
					try {
						const music_lyric_model::parsed::Info info = music_lyric_model::parsed::decodeParsedInfo(*bytes);
						example::persistLyric(*bytes);
						lastHex = *entered;
						load(info, "[loaded]");
					} catch (const std::exception& error) {
						example::reportError(window.hwnd(), std::string("Failed to decode the lyric: ") + error.what());
					}
					break;
				}
			}
		}

		if (window.pollResized()) {
			surface->onResize();
		}
		player.tick();
		surface->renderFrame([&](SkCanvas* canvas) {
			renderer.setViewport(surface->width(), surface->height(), surface->devicePixelRatio());
			renderer.render(canvas);
		});
	}

	return 0;
}
