#include <cstdio>

#include "include/core/SkFontMgr.h"
#include "include/ports/SkTypeface_win.h"
#include "playback/player.h"
#include "render/config/index.h"
#include "render/renderer.h"
#include "sample_lyric.h"
#include "vulkan_window.h"

int main() {
	example::VulkanWindow window;
	if (!window.init(1280, 800, "music-lyric-player - skia")) {
		std::fprintf(stderr, "[example] failed to initialise the Vulkan window\n");
		return 1;
	}

	sk_sp<SkFontMgr> fontMgr = SkFontMgr_New_DirectWrite();

	music_lyric_player::playback::Player player;
	music_lyric_player::render::Renderer renderer(player, fontMgr, player.clock());

	// DirectWrite cannot resolve SkParagraph's default "sans-serif" family, so pick a concrete
	// system family (covers CJK and latin); also centre-align the lyrics for the demo.
	renderer.config.modify([](music_lyric_player::render::config::Root& cfg) {
		cfg.layout.align                      = music_lyric_player::render::config::layout::Align::Left;
		cfg.line.normal.base.font.size        = "42px";
		cfg.line.normal.base.font.family      = "MiSans";
		cfg.line.normal.base.style.normal.color = "rgb(255, 255, 61)";
		cfg.line.normal.base.style.active.color = "#ffffff";
		cfg.scroll.animation.mode             = music_lyric_player::render::config::scroll::Mode::Stagger;
		cfg.scroll.animation.stagger.duration = 500;
		cfg.scroll.animation.stagger.easing   = "ease";
		cfg.scroll.animation.stagger.step     = 40;
		cfg.scroll.animation.stagger.range    = 4;
		cfg.line.normal.main.syllable.word.animation.mask.enabled = true;
		cfg.line.normal.main.syllable.word.animation.floating.enabled = true;
	});

	player.updateLyric(example::buildSampleLyric());
	player.play(30000.0);

	while (!window.shouldClose()) {
		window.pollEvents();
		window.drawFrame([&](SkCanvas* canvas, int widthPx, int heightPx, float dpr) {
			player.tick();
			renderer.setViewport(widthPx, heightPx, dpr);
			renderer.render(canvas);
		});
	}

	return 0;
}
