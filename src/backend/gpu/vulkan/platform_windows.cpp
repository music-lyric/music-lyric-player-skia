#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define VK_USE_PLATFORM_WIN32_KHR

#include <windows.h>

#include "backend/gpu/vulkan/platform.h"
#include "utils/logger/logger.h"

namespace music_lyric_player::backend::gpu::vulkan {
	namespace {
		constexpr utils::Logger logger{"VulkanSurface"};
	} // namespace

	const char* surfaceExtension() {
		return VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
	}

	VkSurfaceKHR createWindowSurface(VkInstance instance, void* window) {
		VkWin32SurfaceCreateInfoKHR createInfo{};
		createInfo.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		createInfo.hinstance = GetModuleHandle(nullptr);
		createInfo.hwnd      = static_cast<HWND>(window);

		VkSurfaceKHR surface = VK_NULL_HANDLE;
		if (vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, &surface) != VK_SUCCESS) {
			logger.error("vkCreateWin32SurfaceKHR failed");
			return VK_NULL_HANDLE;
		}
		return surface;
	}

	// The client area is already in physical pixels for as long as the window is per-monitor DPI aware, which is the host's job to arrange.
	void queryWindowSize(void* window, int& width, int& height) {
		RECT rect{};
		GetClientRect(static_cast<HWND>(window), &rect);
		width  = rect.right - rect.left;
		height = rect.bottom - rect.top;
	}

	// An `HWND` is not refcounted and stays valid until its owner destroys the window, so there is nothing for this pair to hold.
	void retainWindow(void*) {}

	void releaseWindow(void*) {}
} // namespace music_lyric_player::backend::gpu::vulkan
