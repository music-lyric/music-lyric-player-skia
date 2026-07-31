#ifndef MUSIC_LYRIC_PLAYER_BACKEND_GPU_WEBGL_H_
#define MUSIC_LYRIC_PLAYER_BACKEND_GPU_WEBGL_H_

#include <memory>

#include "backend/gpu/surface.h"

namespace music_lyric_player::backend::gpu {
	/**
	 * Creates a surface bound to the canvas that `window.selector` matches, or null on failure.
	 * Each canvas gets a WebGL context and a Skia context of its own, so nothing is shared between surfaces and a page driving several players pays for several GPU stacks.
	 * Surfaces are not thread safe and must be created and driven from the thread that owns the canvas.
	 */
	std::unique_ptr<Surface> createWebglSurface(const NativeWindow& window);
} // namespace music_lyric_player::backend::gpu

#endif // MUSIC_LYRIC_PLAYER_BACKEND_GPU_WEBGL_H_
