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
		LoadHex,
	};

	/**
	 * A thin GLFW window shell that owns no GPU state.
	 * It exposes the native `HWND` so the backend surface can build and drive its own Vulkan swapchain,
	 * and reports resize events so the caller can rebuild that swapchain on the next frame.
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
		 * Reports whether the window has been asked to close.
		 */
		bool shouldClose() const;

		/**
		 * Pumps the platform event queue once.
		 */
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
		 * Returns the navigation actions queued from key presses since the last call, clearing the queue.
		 */
		std::vector<InputAction> drainActions();

		/**
		 * Updates the window title bar text.
		 */
		void setTitle(const char* title);

	private:
		GLFWwindow*              window  = nullptr;
		bool                     resized = false;
		std::vector<InputAction> actions;
	};
} // namespace example

#endif // MUSIC_LYRIC_PLAYER_EXAMPLE_WINDOW_H_
