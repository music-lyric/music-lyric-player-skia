#ifndef MUSIC_LYRIC_PLAYER_PLATFORM_ANDROID_SURFACE_H_
#define MUSIC_LYRIC_PLAYER_PLATFORM_ANDROID_SURFACE_H_

#include <memory>

#include "backend/gpu/surface.h"

struct ANativeWindow;

namespace music_lyric_player::platform::android {
	/**
	 * Creates a GPU surface drawing into `window`, preferring Vulkan and falling back to GLES, or null when neither backend starts.
	 * Vulkan has been optional on Android ever since it arrived there, so a device may carry no driver at all; the fallback is what keeps the library usable on those, not a safety net.
	 * The first surface settles which backend the whole process draws through and every later one joins it, since two backends side by side would mean two GPU stacks and two Skia caches for no gain.
	 * Surfaces are not thread safe, so this is called on the one thread that drives them.
	 */
	std::unique_ptr<backend::gpu::Surface> createSurface(ANativeWindow* window);
} // namespace music_lyric_player::platform::android

#endif // MUSIC_LYRIC_PLAYER_PLATFORM_ANDROID_SURFACE_H_
