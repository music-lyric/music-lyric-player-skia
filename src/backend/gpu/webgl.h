#ifndef MUSIC_LYRIC_PLAYER_BACKEND_GPU_WEBGL_H_
#define MUSIC_LYRIC_PLAYER_BACKEND_GPU_WEBGL_H_

#include <memory>

#include "backend/gpu/surface.h"

namespace music_lyric_player::backend::gpu {
	/**
	 * A surface drawing into an HTML canvas through a WebGL 2 context.
	 * There is no swapchain to present to: a frame ends once its GPU work is flushed, and the browser composites the canvas by itself after the animation frame callback returns.
	 */
	class CanvasSurface : public Surface {
	public:
		/**
		 * Resizes the canvas backbuffer to `width` by `height` physical pixels and records `devicePixelRatio`.
		 * The page owns layout, so only it can know the CSS size and the ratio to scale it by, whereas every other platform reads both back from the window it was handed.
		 */
		virtual void configure(int width, int height, float devicePixelRatio) = 0;
	};

	/**
	 * Creates a surface bound to the canvas that `window.selector` matches, or null on failure.
	 * Each canvas gets a WebGL context and a Skia context of its own, so nothing is shared between surfaces and a page driving several players pays for several GPU stacks.
	 * Surfaces are not thread safe and must be created and driven from the thread that owns the canvas.
	 */
	std::unique_ptr<CanvasSurface> createCanvasSurface(const NativeWindow& window);
} // namespace music_lyric_player::backend::gpu

#endif // MUSIC_LYRIC_PLAYER_BACKEND_GPU_WEBGL_H_
