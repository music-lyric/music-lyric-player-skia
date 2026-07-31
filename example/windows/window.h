#ifndef MUSIC_LYRIC_PLAYER_EXAMPLE_WINDOW_H_
#define MUSIC_LYRIC_PLAYER_EXAMPLE_WINDOW_H_

#include <vector>

struct GLFWwindow;

namespace example {
	/**
	 * A navigation intent decoded from a key press, used to drive the demo's test-case switching.
	 */
	enum class InputAction {
		Restart,
		TogglePause,
		TogglePanel,
		LoadHex,
		LoadAudio,
		SeekBackward,
		SeekForward,
	};

	/**
	 * A thin GLFW window shell that owns no GPU state.
	 * It exposes the native `HWND` so the backend surface can build and drive its own Vulkan swapchain, and reports resize events so the caller can rebuild that swapchain on the next frame.
	 */
	class Window {
	public:
		Window();
		~Window();

		Window(const Window&)            = delete;
		Window& operator=(const Window&) = delete;

		/**
		 * Marks the process per-monitor DPI aware, then creates the borderless-API GLFW window.
		 * Returns false when GLFW or window creation fails.
		 */
		bool init(int width, int height, const char* title);

		/**
		 * Returns the underlying GLFW window so an input backend can attach its own callbacks.
		 */
		GLFWwindow* handle() const;

		bool shouldClose() const;

		void pollEvents();

		/**
		 * Returns the native `HWND` as an opaque pointer for `backend::gpu::NativeWindow`.
		 */
		void* hwnd() const;

		/**
		 * Returns whether the framebuffer was resized since the last call, clearing the flag.
		 */
		bool pollResized();

		/**
		 * Reads the framebuffer size in physical pixels.
		 */
		void framebufferSize(int& width, int& height) const;

		/**
		 * Returns the window's device-pixel ratio, in physical pixels per logical unit.
		 * The backend does not track it, so the host reads it here and passes it on to whatever scales by it.
		 */
		float devicePixelRatio() const;

		/**
		 * Returns the navigation actions queued from key presses since the last call, clearing the queue.
		 */
		std::vector<InputAction> drainActions();

		void setTitle(const char* title);

	private:
		GLFWwindow*              window  = nullptr;
		bool                     resized = false;
		std::vector<InputAction> actions;
	};
} // namespace example

#endif // MUSIC_LYRIC_PLAYER_EXAMPLE_WINDOW_H_
