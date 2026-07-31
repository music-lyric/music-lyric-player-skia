#ifndef MUSIC_LYRIC_PLAYER_BACKEND_GPU_GLES_GLES_H_
#define MUSIC_LYRIC_PLAYER_BACKEND_GPU_GLES_GLES_H_

#include <memory>

#include "backend/gpu/surface.h"

namespace music_lyric_player::backend::gpu::gles {
	/**
	 * Creates a GLES surface drawing into `window.handle`, an `ANativeWindow*`, or null on failure.
	 * Each surface owns its EGL window surface, while the EGL display, context and Skia context are shared with every other live surface.
	 * The shared stack is built by the first surface and freed with the last.
	 * Surfaces are not thread safe and all of them must be created and driven from the same thread, which is also the thread the EGL context stays current on.
	 */
	std::unique_ptr<Surface> createSurface(const NativeWindow& window);
} // namespace music_lyric_player::backend::gpu::gles

#endif // MUSIC_LYRIC_PLAYER_BACKEND_GPU_GLES_GLES_H_
