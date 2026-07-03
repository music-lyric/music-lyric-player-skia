#ifndef MUSIC_LYRIC_PLAYER_EXAMPLE_VULKAN_WINDOW_H_
#define MUSIC_LYRIC_PLAYER_EXAMPLE_VULKAN_WINDOW_H_

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "include/core/SkRefCnt.h"
#include "include/gpu/vk/VulkanExtensions.h"

class GrDirectContext;
class SkCanvas;
class SkSurface;

namespace example {
	/**
	 * A GLFW window backed by a Vulkan swapchain whose images are wrapped as Skia surfaces.
	 * Owns the whole GPU stack (instance / device / swapchain / GrDirectContext) and drives frames.
	 * Sync is intentionally simple: one queue-wait-idle per frame, no pipelining — enough for a demo.
	 */
	class VulkanWindow {
	public:
		/**
		 * Per-frame draw callback: receives the surface canvas plus its pixel size and device-pixel ratio.
		 */
		using DrawCallback = std::function<void(SkCanvas*, int, int, float)>;

		VulkanWindow();
		~VulkanWindow();

		VulkanWindow(const VulkanWindow&)            = delete;
		VulkanWindow& operator=(const VulkanWindow&) = delete;

		/**
		 * Creates the window and the full Vulkan + Skia stack; returns false on any failure.
		 */
		bool init(int width, int height, const char* title);

		/**
		 * Whether the user asked to close the window.
		 */
		bool shouldClose() const;

		/**
		 * Pumps the window's event queue.
		 */
		void pollEvents();

		/**
		 * Acquires the next swapchain image, runs `draw`, then flushes and presents it.
		 */
		void drawFrame(const DrawCallback& draw);

	private:
		/**
		 * Creates the Vulkan instance with the extensions GLFW requires for presentation.
		 */
		bool createInstance();

		/**
		 * Creates the window surface from the GLFW window.
		 */
		bool createSurface();

		/**
		 * Picks the first physical device whose queue family supports graphics and presentation.
		 */
		bool pickPhysicalDevice();

		/**
		 * Creates the logical device, the graphics/present queue and the swapchain extension.
		 */
		bool createDevice();

		/**
		 * Builds the Skia Vulkan backend context and the GrDirectContext.
		 */
		bool createSkiaContext();

		/**
		 * Creates the swapchain and wraps each image as an SkSurface.
		 */
		bool createSwapchain();

		/**
		 * Releases the swapchain and its wrapped surfaces.
		 */
		void destroySwapchain();

		/**
		 * Waits for the device to idle, then rebuilds the swapchain (used on resize / out-of-date).
		 */
		bool recreateSwapchain();

		/**
		 * Tears down the whole stack in reverse creation order.
		 */
		void cleanup();

		/**
		 * Ratio of framebuffer pixels to logical window units (high-DPI scale).
		 */
		float queryDpr() const;

		GLFWwindow*      window_           = nullptr;
		VkInstance       instance_         = VK_NULL_HANDLE;
		VkSurfaceKHR     surface_          = VK_NULL_HANDLE;
		VkPhysicalDevice physicalDevice_   = VK_NULL_HANDLE;
		VkDevice         device_           = VK_NULL_HANDLE;
		std::uint32_t    queueFamilyIndex_ = 0;
		VkQueue          queue_            = VK_NULL_HANDLE;

		VkPhysicalDeviceFeatures2 deviceFeatures2_{}; // queried device features, handed to Skia

		skgpu::VulkanExtensions extensions_;
		sk_sp<GrDirectContext>  context_;

		VkSwapchainKHR                swapchain_       = VK_NULL_HANDLE;
		VkFormat                      swapchainFormat_ = VK_FORMAT_UNDEFINED;
		VkImageUsageFlags             swapchainUsage_  = 0;
		VkExtent2D                    swapchainExtent_ = {0, 0};
		std::vector<VkImage>          swapchainImages_;
		std::vector<sk_sp<SkSurface>> surfaces_;

		std::vector<std::string> instanceExtensionNames_;
		std::vector<std::string> deviceExtensionNames_;

		bool framebufferResized_ = false;
	};
} // namespace example

#endif
