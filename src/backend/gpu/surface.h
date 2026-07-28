#ifndef MUSIC_LYRIC_PLAYER_BACKEND_GPU_SURFACE_H_
#define MUSIC_LYRIC_PLAYER_BACKEND_GPU_SURFACE_H_

#include <functional>
#include <memory>

class SkCanvas;

namespace music_lyric_player::backend::gpu {
	/**
	 * Opaque handle to the native window a surface draws into.
	 * On Windows this carries an `HWND` and on the web a CSS selector picking the canvas element;
	 * other platforms extend it with their own fields.
	 */
	struct NativeWindow {
		void*       hwnd     = nullptr;
		const char* selector = nullptr;
	};

	/**
	 * A GPU-backed drawing target bound to a native window.
	 * Owns its swapchain and is driven once per frame by the caller, drawing through a shared GPU context.
	 * The renderer never sees this type; the platform layer drives it and hands the canvas to the renderer.
	 */
	class Surface {
	public:
		virtual ~Surface() = default;

		/**
		 * Draws one frame: acquires a backbuffer, invokes `paint` with its canvas, then presents it.
		 * When the surface cannot present (window minimized or swapchain out of date), `paint` is skipped.
		 */
		virtual void renderFrame(const std::function<void(SkCanvas*)>& paint) = 0;

		/**
		 * Rebuilds the swapchain against the window's current client size; call after the window resized.
		 */
		virtual void onResize() = 0;

		/**
		 * Returns the current backbuffer width in physical pixels.
		 */
		virtual int width() const = 0;

		/**
		 * Returns the current backbuffer height in physical pixels.
		 */
		virtual int height() const = 0;

		/**
		 * Returns the window's device-pixel ratio (physical pixels per logical unit).
		 */
		virtual float devicePixelRatio() const = 0;
	};

	/**
	 * Creates a window-owned Vulkan surface for `window`, or null on failure.
	 * The surface owns its swapchain, while the Vulkan instance, device and Skia context are shared
	 * with every other live surface; the shared stack is built by the first one and freed with the last.
	 * Surfaces are not thread safe and all of them must be created and driven from the same thread.
	 */
	std::unique_ptr<Surface> createWindowSurface(const NativeWindow& window);
} // namespace music_lyric_player::backend::gpu

#endif // MUSIC_LYRIC_PLAYER_BACKEND_GPU_SURFACE_H_
