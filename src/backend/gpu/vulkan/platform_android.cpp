#define VK_USE_PLATFORM_ANDROID_KHR

#include <android/native_window.h>

#include "backend/gpu/vulkan/platform.h"
#include "utils/logger/logger.h"

namespace music_lyric_player::backend::gpu::vulkan {
	namespace {
		constexpr utils::Logger logger{"VulkanSurface"};
	} // namespace

	const char* surfaceExtension() {
		return VK_KHR_ANDROID_SURFACE_EXTENSION_NAME;
	}

	VkSurfaceKHR createWindowSurface(VkInstance instance, void* window) {
		VkAndroidSurfaceCreateInfoKHR createInfo{};
		createInfo.sType  = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
		createInfo.window = static_cast<ANativeWindow*>(window);

		VkSurfaceKHR surface = VK_NULL_HANDLE;
		if (vkCreateAndroidSurfaceKHR(instance, &createInfo, nullptr, &surface) != VK_SUCCESS) {
			logger.error("vkCreateAndroidSurfaceKHR failed");
			return VK_NULL_HANDLE;
		}
		return surface;
	}

	// The window reports the size it was configured with, which trails the buffer by a frame or two around a rotation.
	// That is survivable only because the first swapchain is the one asking; every later one is sized from the surface capabilities instead.
	void queryWindowSize(void* window, int& width, int& height) {
		auto* native = static_cast<ANativeWindow*>(window);
		width        = ANativeWindow_getWidth(native);
		height       = ANativeWindow_getHeight(native);
	}

	// An `ANativeWindow` is refcounted, so the surface takes a reference of its own rather than trusting the host to outlive it.
	void retainWindow(void* window) {
		ANativeWindow_acquire(static_cast<ANativeWindow*>(window));
	}

	void releaseWindow(void* window) {
		ANativeWindow_release(static_cast<ANativeWindow*>(window));
	}
} // namespace music_lyric_player::backend::gpu::vulkan
