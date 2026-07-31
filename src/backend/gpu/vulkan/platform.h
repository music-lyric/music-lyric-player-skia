#ifndef MUSIC_LYRIC_PLAYER_BACKEND_GPU_VULKAN_PLATFORM_H_
#define MUSIC_LYRIC_PLAYER_BACKEND_GPU_VULKAN_PLATFORM_H_

#include <vulkan/vulkan.h>

namespace music_lyric_player::backend::gpu::vulkan {
	// Everything a swapchain needs from the window system; these five calls are the only places the two differ.
	// `src/CMakeLists.txt` builds exactly one implementation into the module, so nothing above this layer ever names a window system.
	// Every `window` below is the pointer `NativeWindow::handle` carried in: an `HWND` on Windows, an `ANativeWindow*` on Android.

	/**
	 * The instance extension this window system has to have enabled before it can be presented to, asked for next to `VK_KHR_surface`.
	 */
	const char* surfaceExtension();

	/**
	 * Creates the Vulkan surface backing `window`, or a null handle on failure.
	 */
	VkSurfaceKHR createWindowSurface(VkInstance instance, void* window);

	/**
	 * Reads the window's drawable area in physical pixels.
	 * Only the first swapchain has to ask: once a surface exists, the extent it reports is the more current answer.
	 */
	void queryWindowSize(void* window, int& width, int& height);

	/**
	 * Holds `window` alive for as long as a surface draws into it, on the window systems that refcount their windows.
	 * The host owns a reference of its own and may drop it the moment it has handed the window over, so this is what keeps a surface from drawing into freed memory.
	 */
	void retainWindow(void* window);

	/**
	 * Drops what `retainWindow` took.
	 */
	void releaseWindow(void* window);
} // namespace music_lyric_player::backend::gpu::vulkan

#endif // MUSIC_LYRIC_PLAYER_BACKEND_GPU_VULKAN_PLATFORM_H_
