#include <cstdio>

#include "include/core/SkFontMgr.h"
#include "backend/font/font.h"
#include "backend/gpu/surface.h"
#include "playback/player.h"
#include "rendering/config/index.h"
#include "rendering/renderer.h"
#include "sample_lyric.h"
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

	player.updateLyric(example::buildSampleLyric());
	player.play(30000.0);

	while (!window.shouldClose()) {
		window.pollEvents();
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
