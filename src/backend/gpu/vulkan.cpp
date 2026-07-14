#include "backend/gpu/surface.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
	#define NOMINMAX
#endif
#include <windows.h>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include "include/core/SkCanvas.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkColorType.h"
#include "include/core/SkSurface.h"
#include "include/gpu/MutableTextureState.h"
#include "include/gpu/ganesh/GrBackendSemaphore.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/GrTypes.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/vk/GrVkBackendSemaphore.h"
#include "include/gpu/ganesh/vk/GrVkBackendSurface.h"
#include "include/gpu/ganesh/vk/GrVkDirectContext.h"
#include "include/gpu/ganesh/vk/GrVkTypes.h"
#include "include/gpu/vk/VulkanBackendContext.h"
#include "include/gpu/vk/VulkanExtensions.h"
#include "include/gpu/vk/VulkanMutableTextureState.h"
#include "include/gpu/vk/VulkanTypes.h"

namespace music_lyric_player::backend::gpu {
	namespace {
		/**
		 * Resolves a Vulkan entry point from the device when available, else from the instance.
		 */
		PFN_vkVoidFunction vulkanGetProc(const char* name, VkInstance instance, VkDevice device) {
			if (device != VK_NULL_HANDLE) {
				return vkGetDeviceProcAddr(device, name);
			}
			return vkGetInstanceProcAddr(instance, name);
		}

		/**
		 * A Vulkan swapchain bound to a Win32 `HWND`, exposing each backbuffer as a Skia surface.
		 * Owns the whole GPU stack (instance / device / swapchain / GrDirectContext) and drives one frame at a time.
		 * Sync is intentionally simple: one queue-wait-idle per frame, no pipelining — enough for this use.
		 * Assumes the window is per-monitor DPI aware, so its client rect is already in physical pixels.
		 */
		class WindowSurface : public Surface {
		public:
			/**
			 * Builds the full Vulkan + Skia stack for `hwnd`; returns false on any failure.
			 */
			bool init(HWND hwnd);

			~WindowSurface() override {
				cleanup();
			}

			void renderFrame(const std::function<void(SkCanvas*)>& paint) override;
			void onResize() override;

			int width() const override {
				return static_cast<int>(this->swapchainExtent.width);
			}

			int height() const override {
				return static_cast<int>(this->swapchainExtent.height);
			}

			float devicePixelRatio() const override;

		private:
			/**
			 * Acquires the next swapchain image and returns its canvas, or null when the frame must be skipped.
			 */
			SkCanvas* acquire();

			/**
			 * Flushes and presents the frame that the last `acquire` returned a canvas for.
			 */
			void present();

			/**
			 * Creates the Vulkan instance with the surface extensions Win32 presentation needs.
			 */
			bool createInstance();

			/**
			 * Creates the `VkSurfaceKHR` from the owned `HWND`.
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
			 * Creates the swapchain against the window's current size and wraps each image as an SkSurface.
			 */
			bool createSwapchain();

			/**
			 * Destroys the swapchain and the Skia surfaces wrapping its images.
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
			 * Reads the window's client area in physical pixels.
			 */
			void queryClientSize(int& width, int& height) const;

			HWND             hwnd             = nullptr;
			VkInstance       instance         = VK_NULL_HANDLE;
			VkSurfaceKHR     surface          = VK_NULL_HANDLE;
			VkPhysicalDevice physicalDevice   = VK_NULL_HANDLE;
			VkDevice         device           = VK_NULL_HANDLE;
			std::uint32_t    queueFamilyIndex = 0;
			VkQueue          queue            = VK_NULL_HANDLE;

			VkPhysicalDeviceFeatures2 deviceFeatures2{}; // queried device features, handed to Skia

			skgpu::VulkanExtensions extensions;
			sk_sp<GrDirectContext>  context;

			VkSwapchainKHR                swapchain       = VK_NULL_HANDLE;
			VkFormat                      swapchainFormat = VK_FORMAT_UNDEFINED;
			VkExtent2D                    swapchainExtent = {0, 0};
			std::vector<VkImage>          swapchainImages;
			std::vector<sk_sp<SkSurface>> surfaces;

			std::vector<std::string> instanceExtensionNames;
			std::vector<std::string> deviceExtensionNames;

			bool          needRecreate      = false; // rebuild the swapchain before the next frame
			std::uint32_t currentImageIndex = 0;     // backbuffer index acquire selected, presented by present
			SkSurface*    currentSurface    = nullptr;
		};

		bool WindowSurface::init(HWND hwnd) {
			this->hwnd = hwnd;
			return createInstance() && createSurface() && pickPhysicalDevice() && createDevice() &&
				createSkiaContext() && createSwapchain();
		}

		bool WindowSurface::createInstance() {
			this->instanceExtensionNames = {
				VK_KHR_SURFACE_EXTENSION_NAME,
				VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
			};
			std::vector<const char*> enabled;
			enabled.reserve(this->instanceExtensionNames.size());
			for (const std::string& name : this->instanceExtensionNames) {
				enabled.push_back(name.c_str());
			}

			// pApplicationName / applicationVersion are the consumer app's; a library must not claim them.
			VkApplicationInfo appInfo{};
			appInfo.sType         = VK_STRUCTURE_TYPE_APPLICATION_INFO;
			appInfo.pEngineName   = "music-lyric-player";
			appInfo.engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
			appInfo.apiVersion    = VK_API_VERSION_1_1;

			VkInstanceCreateInfo createInfo{};
			createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
			createInfo.pApplicationInfo        = &appInfo;
			createInfo.enabledExtensionCount   = static_cast<std::uint32_t>(enabled.size());
			createInfo.ppEnabledExtensionNames = enabled.data();

			if (vkCreateInstance(&createInfo, nullptr, &this->instance) != VK_SUCCESS) {
				std::fprintf(stderr, "[backend] vkCreateInstance failed\n");
				return false;
			}
			return true;
		}

		bool WindowSurface::createSurface() {
			VkWin32SurfaceCreateInfoKHR createInfo{};
			createInfo.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
			createInfo.hinstance = GetModuleHandle(nullptr);
			createInfo.hwnd      = this->hwnd;

			if (vkCreateWin32SurfaceKHR(this->instance, &createInfo, nullptr, &this->surface) != VK_SUCCESS) {
				std::fprintf(stderr, "[backend] vkCreateWin32SurfaceKHR failed\n");
				return false;
			}
			return true;
		}

		bool WindowSurface::pickPhysicalDevice() {
			std::uint32_t deviceCount = 0;
			vkEnumeratePhysicalDevices(this->instance, &deviceCount, nullptr);
			if (deviceCount == 0) {
				std::fprintf(stderr, "[backend] no Vulkan physical devices found\n");
				return false;
			}
			std::vector<VkPhysicalDevice> devices(deviceCount);
			vkEnumeratePhysicalDevices(this->instance, &deviceCount, devices.data());

			for (VkPhysicalDevice candidate : devices) {
				std::uint32_t familyCount = 0;
				vkGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount, nullptr);
				std::vector<VkQueueFamilyProperties> families(familyCount);
				vkGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount, families.data());

				for (std::uint32_t i = 0; i < familyCount; ++i) {
					VkBool32 presentSupport = VK_FALSE;
					vkGetPhysicalDeviceSurfaceSupportKHR(candidate, i, this->surface, &presentSupport);
					const bool graphics = (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
					if (graphics && presentSupport == VK_TRUE) {
						this->physicalDevice   = candidate;
						this->queueFamilyIndex = i;
						return true;
					}
				}
			}
			std::fprintf(stderr, "[backend] no graphics+present queue family found\n");
			return false;
		}

		bool WindowSurface::createDevice() {
			const float             queuePriority = 1.0f;
			VkDeviceQueueCreateInfo queueInfo{};
			queueInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueInfo.queueFamilyIndex = this->queueFamilyIndex;
			queueInfo.queueCount       = 1;
			queueInfo.pQueuePriorities = &queuePriority;

			this->deviceExtensionNames = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
			std::vector<const char*> enabled;
			enabled.reserve(this->deviceExtensionNames.size());
			for (const std::string& name : this->deviceExtensionNames) {
				enabled.push_back(name.c_str());
			}

			// Enable all device-supported features through a features2 chain (Skia consumes the same
			// struct as fDeviceFeatures2); pEnabledFeatures must stay null when pNext carries features2.
			this->deviceFeatures2       = VkPhysicalDeviceFeatures2{};
			this->deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
			vkGetPhysicalDeviceFeatures2(this->physicalDevice, &this->deviceFeatures2);

			VkDeviceCreateInfo createInfo{};
			createInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
			createInfo.pNext                   = &this->deviceFeatures2;
			createInfo.queueCreateInfoCount    = 1;
			createInfo.pQueueCreateInfos       = &queueInfo;
			createInfo.enabledExtensionCount   = static_cast<std::uint32_t>(enabled.size());
			createInfo.ppEnabledExtensionNames = enabled.data();

			if (vkCreateDevice(this->physicalDevice, &createInfo, nullptr, &this->device) != VK_SUCCESS) {
				std::fprintf(stderr, "[backend] vkCreateDevice failed\n");
				return false;
			}
			vkGetDeviceQueue(this->device, this->queueFamilyIndex, 0, &this->queue);
			return true;
		}

		bool WindowSurface::createSkiaContext() {
			std::vector<const char*> instanceExtensions;
			std::vector<const char*> deviceExtensions;
			for (const std::string& name : this->instanceExtensionNames) {
				instanceExtensions.push_back(name.c_str());
			}
			for (const std::string& name : this->deviceExtensionNames) {
				deviceExtensions.push_back(name.c_str());
			}

			this->extensions.init(&vulkanGetProc,
				this->instance,
				this->physicalDevice,
				static_cast<std::uint32_t>(instanceExtensions.size()),
				instanceExtensions.data(),
				static_cast<std::uint32_t>(deviceExtensions.size()),
				deviceExtensions.data());

			skgpu::VulkanBackendContext backend{};
			backend.fInstance           = this->instance;
			backend.fPhysicalDevice     = this->physicalDevice;
			backend.fDevice             = this->device;
			backend.fQueue              = this->queue;
			backend.fGraphicsQueueIndex = this->queueFamilyIndex;
			backend.fMaxAPIVersion      = VK_API_VERSION_1_1;
			backend.fVkExtensions       = &this->extensions;
			backend.fDeviceFeatures2    = &this->deviceFeatures2;
			backend.fGetProc            = &vulkanGetProc;
			// Leave fMemoryAllocator null so Skia builds its bundled VMA allocator (Skia is built with SK_USE_VMA).

			this->context = GrDirectContexts::MakeVulkan(backend);
			if (this->context == nullptr) {
				std::fprintf(stderr, "[backend] GrDirectContexts::MakeVulkan failed\n");
				return false;
			}
			return true;
		}

		bool WindowSurface::createSwapchain() {
			VkSurfaceCapabilitiesKHR capabilities{};
			vkGetPhysicalDeviceSurfaceCapabilitiesKHR(this->physicalDevice, this->surface, &capabilities);

			std::uint32_t formatCount = 0;
			vkGetPhysicalDeviceSurfaceFormatsKHR(this->physicalDevice, this->surface, &formatCount, nullptr);
			if (formatCount == 0) {
				std::fprintf(stderr, "[backend] no surface formats\n");
				return false;
			}
			std::vector<VkSurfaceFormatKHR> formats(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(this->physicalDevice, this->surface, &formatCount, formats.data());

			VkSurfaceFormatKHR chosen = formats[0];
			for (const VkSurfaceFormatKHR& format : formats) {
				if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
					format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
					chosen = format;
					break;
				}
			}
			this->swapchainFormat = chosen.format;

			int clientWidth  = 0;
			int clientHeight = 0;
			queryClientSize(clientWidth, clientHeight);

			VkExtent2D extent = capabilities.currentExtent;
			if (capabilities.currentExtent.width == UINT32_MAX) {
				extent.width  = std::clamp(static_cast<std::uint32_t>(clientWidth),
					capabilities.minImageExtent.width,
					capabilities.maxImageExtent.width);
				extent.height = std::clamp(static_cast<std::uint32_t>(clientHeight),
					capabilities.minImageExtent.height,
					capabilities.maxImageExtent.height);
			}
			if (extent.width == 0 || extent.height == 0) {
				return false; // minimized; acquire skips the frame until the window is restored
			}
			this->swapchainExtent = extent;

			std::uint32_t imageCount = capabilities.minImageCount + 1;
			if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
				imageCount = capabilities.maxImageCount;
			}

			VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
				VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			usage &= capabilities.supportedUsageFlags;

			VkSwapchainCreateInfoKHR createInfo{};
			createInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
			createInfo.surface          = this->surface;
			createInfo.minImageCount    = imageCount;
			createInfo.imageFormat      = chosen.format;
			createInfo.imageColorSpace  = chosen.colorSpace;
			createInfo.imageExtent      = extent;
			createInfo.imageArrayLayers = 1;
			createInfo.imageUsage       = usage;
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.preTransform     = capabilities.currentTransform;
			createInfo.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
			createInfo.presentMode      = VK_PRESENT_MODE_FIFO_KHR; // always supported
			createInfo.clipped          = VK_TRUE;
			createInfo.oldSwapchain     = VK_NULL_HANDLE;

			if (vkCreateSwapchainKHR(this->device, &createInfo, nullptr, &this->swapchain) != VK_SUCCESS) {
				std::fprintf(stderr, "[backend] vkCreateSwapchainKHR failed\n");
				return false;
			}

			vkGetSwapchainImagesKHR(this->device, this->swapchain, &imageCount, nullptr);
			this->swapchainImages.resize(imageCount);
			vkGetSwapchainImagesKHR(this->device, this->swapchain, &imageCount, this->swapchainImages.data());

			const SkColorType colorType = this->swapchainFormat == VK_FORMAT_B8G8R8A8_UNORM
				? kBGRA_8888_SkColorType
				: kRGBA_8888_SkColorType;

			this->surfaces.assign(imageCount, nullptr);
			for (std::uint32_t i = 0; i < imageCount; ++i) {
				GrVkImageInfo imageInfo;
				imageInfo.fImage              = this->swapchainImages[i];
				imageInfo.fImageTiling        = VK_IMAGE_TILING_OPTIMAL;
				imageInfo.fImageLayout        = VK_IMAGE_LAYOUT_UNDEFINED;
				imageInfo.fFormat             = this->swapchainFormat;
				imageInfo.fImageUsageFlags    = usage;
				imageInfo.fSampleCount        = 1;
				imageInfo.fLevelCount         = 1;
				imageInfo.fCurrentQueueFamily = VK_QUEUE_FAMILY_IGNORED;
				imageInfo.fSharingMode        = VK_SHARING_MODE_EXCLUSIVE;

				GrBackendRenderTarget renderTarget = GrBackendRenderTargets::MakeVk(
					static_cast<int>(extent.width),
					static_cast<int>(extent.height),
					imageInfo);

				this->surfaces[i] = SkSurfaces::WrapBackendRenderTarget(this->context.get(),
					renderTarget,
					kTopLeft_GrSurfaceOrigin,
					colorType,
					nullptr,
					nullptr);
				if (this->surfaces[i] == nullptr) {
					std::fprintf(stderr, "[backend] WrapBackendRenderTarget failed\n");
					return false;
				}
			}
			return true;
		}

		void WindowSurface::destroySwapchain() {
			this->surfaces.clear();
			this->swapchainImages.clear();
			if (this->swapchain != VK_NULL_HANDLE) {
				vkDestroySwapchainKHR(this->device, this->swapchain, nullptr);
				this->swapchain = VK_NULL_HANDLE;
			}
		}

		bool WindowSurface::recreateSwapchain() {
			if (this->device != VK_NULL_HANDLE) {
				vkDeviceWaitIdle(this->device);
			}
			destroySwapchain();
			return createSwapchain();
		}

		void WindowSurface::renderFrame(const std::function<void(SkCanvas*)>& paint) {
			SkCanvas* canvas = acquire();
			if (canvas == nullptr) {
				return; // minimized or swapchain out of date; skip this frame
			}
			paint(canvas);
			present();
		}

		SkCanvas* WindowSurface::acquire() {
			if (this->needRecreate || this->swapchain == VK_NULL_HANDLE) {
				this->needRecreate = false;
				if (!recreateSwapchain()) {
					return nullptr; // minimized or failed
				}
			}

			VkSemaphoreCreateInfo semaphoreInfo{};
			semaphoreInfo.sType          = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
			VkSemaphore acquireSemaphore = VK_NULL_HANDLE;
			if (vkCreateSemaphore(this->device, &semaphoreInfo, nullptr, &acquireSemaphore) != VK_SUCCESS) {
				return nullptr;
			}

			std::uint32_t  imageIndex = 0;
			const VkResult result     = vkAcquireNextImageKHR(this->device,
				this->swapchain,
				UINT64_MAX,
				acquireSemaphore,
				VK_NULL_HANDLE,
				&imageIndex);
			if (result == VK_ERROR_OUT_OF_DATE_KHR) {
				vkDestroySemaphore(this->device, acquireSemaphore, nullptr);
				this->needRecreate = true;
				return nullptr;
			}
			if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
				vkDestroySemaphore(this->device, acquireSemaphore, nullptr);
				return nullptr;
			}

			SkSurface* target = this->surfaces[imageIndex].get();

			// Have Skia's GPU work wait on the acquire semaphore; Skia takes ownership and deletes it.
			GrBackendSemaphore backendAcquire = GrBackendSemaphores::MakeVk(acquireSemaphore);
			if (!target->wait(1, &backendAcquire)) {
				vkDestroySemaphore(this->device, acquireSemaphore, nullptr);
				return nullptr;
			}

			this->currentImageIndex = imageIndex;
			this->currentSurface    = target;
			return target->getCanvas();
		}

		void WindowSurface::present() {
			GrFlushInfo                flushInfo{};
			skgpu::MutableTextureState presentState = skgpu::MutableTextureStates::MakeVulkan(
				VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				this->queueFamilyIndex);
			this->context->flush(this->currentSurface, flushInfo, &presentState);
			this->context->submit();

			vkQueueWaitIdle(this->queue);

			VkPresentInfoKHR presentInfo{};
			presentInfo.sType          = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			presentInfo.swapchainCount = 1;
			presentInfo.pSwapchains    = &this->swapchain;
			presentInfo.pImageIndices  = &this->currentImageIndex;

			const VkResult result = vkQueuePresentKHR(this->queue, &presentInfo);
			if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
				this->needRecreate = true;
			}
			this->currentSurface = nullptr;
		}

		void WindowSurface::onResize() {
			this->needRecreate = true;
		}

		float WindowSurface::devicePixelRatio() const {
			const UINT dpi = GetDpiForWindow(this->hwnd);
			return dpi > 0 ? static_cast<float>(dpi) / 96.0f : 1.0f;
		}

		void WindowSurface::cleanup() {
			if (this->device != VK_NULL_HANDLE) {
				vkDeviceWaitIdle(this->device);
			}
			destroySwapchain();
			this->context.reset(); // release the Skia GPU context before its Vulkan device

			if (this->device != VK_NULL_HANDLE) {
				vkDestroyDevice(this->device, nullptr);
				this->device = VK_NULL_HANDLE;
			}
			if (this->surface != VK_NULL_HANDLE) {
				vkDestroySurfaceKHR(this->instance, this->surface, nullptr);
				this->surface = VK_NULL_HANDLE;
			}
			if (this->instance != VK_NULL_HANDLE) {
				vkDestroyInstance(this->instance, nullptr);
				this->instance = VK_NULL_HANDLE;
			}
		}

		void WindowSurface::queryClientSize(int& width, int& height) const {
			RECT rect{};
			GetClientRect(this->hwnd, &rect);
			width  = rect.right - rect.left;
			height = rect.bottom - rect.top;
		}
	} // namespace

	std::unique_ptr<Surface> createWindowSurface(const NativeWindow& window) {
		auto surface = std::make_unique<WindowSurface>();
		if (!surface->init(static_cast<HWND>(window.hwnd))) {
			return nullptr;
		}
		return surface;
	}
} // namespace music_lyric_player::backend::gpu
