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
	renderer.config.modify([](auto& cfg) {
		cfg.line.font.family = "Microsoft YaHei UI";
		cfg.layout.align     = 1;
	});

	player.updateLyric(example::buildSampleLyric());
	player.play(0.0);

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
