#ifndef MUSIC_LYRIC_PLAYER_BACKEND_GPU_SURFACE_H_
#define MUSIC_LYRIC_PLAYER_BACKEND_GPU_SURFACE_H_

#include <functional>

class SkCanvas;

namespace music_lyric_player::backend::gpu {
	/**
	 * Opaque handle to the native window a surface draws into.
	 * `handle` carries the platform's own window pointer, an `HWND` on Windows and an `ANativeWindow*` on Android; the web has no such object and names its canvas with a CSS selector instead.
	 */
	struct NativeWindow {
		void*       handle   = nullptr;
		const char* selector = nullptr;
	};

	/**
	 * A GPU-backed drawing target bound to a native window.
	 * Owns its swapchain and is driven once per frame by the caller, drawing through a shared GPU context.
	 * The renderer never sees this type; the platform layer drives it and hands the canvas to the renderer.
	 * The device-pixel ratio deliberately lives outside this interface: no backend consumes it, and the host that reports a resize already knows it.
	 */
	class Surface {
	public:
		virtual ~Surface() = default;

		/**
		 * Draws one frame: acquires a backbuffer, invokes `draw` with its canvas, then presents it.
		 * When the surface cannot present (window minimized or swapchain out of date), `draw` is skipped.
		 */
		virtual void renderFrame(const std::function<void(SkCanvas*)>& draw) = 0;

		/**
		 * Reports that the drawing area became `width` by `height` physical pixels, rebuilding against it before the next frame.
		 * Backends whose platform reports the drawable size authoritatively verify these numbers and override them; only the web takes them as the truth, because nothing else there knows the page's layout.
		 */
		virtual void handleResize(int width, int height) = 0;

		/**
		 * The current backbuffer width in physical pixels.
		 */
		virtual int width() const = 0;

		/**
		 * The current backbuffer height in physical pixels.
		 */
		virtual int height() const = 0;
	};
} // namespace music_lyric_player::backend::gpu

#endif // MUSIC_LYRIC_PLAYER_BACKEND_GPU_SURFACE_H_
