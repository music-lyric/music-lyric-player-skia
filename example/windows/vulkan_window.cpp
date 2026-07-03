#include "vulkan_window.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

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

namespace example {
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
		 * Clamps a value into an inclusive range.
		 */
		std::uint32_t clampU32(std::uint32_t value, std::uint32_t low, std::uint32_t high) {
			return std::max(low, std::min(value, high));
		}
	} // namespace

	VulkanWindow::VulkanWindow() = default;

	VulkanWindow::~VulkanWindow() {
		cleanup();
	}

	bool VulkanWindow::init(int width, int height, const char* title) {
		if (glfwInit() == GLFW_FALSE) {
			std::fprintf(stderr, "[example] glfwInit failed\n");
			return false;
		}
		if (glfwVulkanSupported() == GLFW_FALSE) {
			std::fprintf(stderr, "[example] Vulkan is not supported by the GLFW loader\n");
			return false;
		}

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		window_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
		if (window_ == nullptr) {
			std::fprintf(stderr, "[example] glfwCreateWindow failed\n");
			return false;
		}

		glfwSetWindowUserPointer(window_, this);
		glfwSetFramebufferSizeCallback(window_, [](GLFWwindow* window, int, int) {
			auto* self = static_cast<VulkanWindow*>(glfwGetWindowUserPointer(window));
			if (self != nullptr) {
				self->framebufferResized_ = true;
			}
		});

		return createInstance() && createSurface() && pickPhysicalDevice() && createDevice() &&
			createSkiaContext() && createSwapchain();
	}

	bool VulkanWindow::shouldClose() const {
		return window_ == nullptr || glfwWindowShouldClose(window_) == GLFW_TRUE;
	}

	void VulkanWindow::pollEvents() {
		glfwPollEvents();
	}

	bool VulkanWindow::createInstance() {
		std::uint32_t      glfwExtensionCount = 0;
		const char* const* glfwExtensions     = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
		if (glfwExtensions == nullptr) {
			std::fprintf(stderr, "[example] glfwGetRequiredInstanceExtensions failed\n");
			return false;
		}
		instanceExtensionNames_.assign(glfwExtensions, glfwExtensions + glfwExtensionCount);

		std::vector<const char*> extensions;
		extensions.reserve(instanceExtensionNames_.size());
		for (const std::string& name : instanceExtensionNames_) {
			extensions.push_back(name.c_str());
		}

		VkApplicationInfo appInfo{};
		appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName   = "music-lyric-player";
		appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 0, 0, 0);
		appInfo.pEngineName        = "music-lyric-player";
		appInfo.engineVersion      = VK_MAKE_API_VERSION(0, 0, 0, 0);
		appInfo.apiVersion         = VK_API_VERSION_1_1;

		VkInstanceCreateInfo createInfo{};
		createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo        = &appInfo;
		createInfo.enabledExtensionCount   = static_cast<std::uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();

		if (vkCreateInstance(&createInfo, nullptr, &instance_) != VK_SUCCESS) {
			std::fprintf(stderr, "[example] vkCreateInstance failed\n");
			return false;
		}
		return true;
	}

	bool VulkanWindow::createSurface() {
		if (glfwCreateWindowSurface(instance_, window_, nullptr, &surface_) != VK_SUCCESS) {
			std::fprintf(stderr, "[example] glfwCreateWindowSurface failed\n");
			return false;
		}
		return true;
	}

	bool VulkanWindow::pickPhysicalDevice() {
		std::uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);
		if (deviceCount == 0) {
			std::fprintf(stderr, "[example] no Vulkan physical devices found\n");
			return false;
		}
		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

		for (VkPhysicalDevice device : devices) {
			std::uint32_t familyCount = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, nullptr);
			std::vector<VkQueueFamilyProperties> families(familyCount);
			vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, families.data());

			for (std::uint32_t i = 0; i < familyCount; ++i) {
				VkBool32 presentSupport = VK_FALSE;
				vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &presentSupport);
				const bool graphics = (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
				if (graphics && presentSupport == VK_TRUE) {
					physicalDevice_   = device;
					queueFamilyIndex_ = i;
					return true;
				}
			}
		}
		std::fprintf(stderr, "[example] no graphics+present queue family found\n");
		return false;
	}

	bool VulkanWindow::createDevice() {
		const float             queuePriority = 1.0f;
		VkDeviceQueueCreateInfo queueInfo{};
		queueInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueInfo.queueFamilyIndex = queueFamilyIndex_;
		queueInfo.queueCount       = 1;
		queueInfo.pQueuePriorities = &queuePriority;

		deviceExtensionNames_ = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
		std::vector<const char*> extensions;
		extensions.reserve(deviceExtensionNames_.size());
		for (const std::string& name : deviceExtensionNames_) {
			extensions.push_back(name.c_str());
		}

		// Enable all device-supported features through a features2 chain (Skia consumes the same
		// struct as fDeviceFeatures2); pEnabledFeatures must stay null when pNext carries features2.
		deviceFeatures2_       = VkPhysicalDeviceFeatures2{};
		deviceFeatures2_.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		vkGetPhysicalDeviceFeatures2(physicalDevice_, &deviceFeatures2_);

		VkDeviceCreateInfo createInfo{};
		createInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.pNext                   = &deviceFeatures2_;
		createInfo.queueCreateInfoCount    = 1;
		createInfo.pQueueCreateInfos       = &queueInfo;
		createInfo.enabledExtensionCount   = static_cast<std::uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();

		if (vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_) != VK_SUCCESS) {
			std::fprintf(stderr, "[example] vkCreateDevice failed\n");
			return false;
		}
		vkGetDeviceQueue(device_, queueFamilyIndex_, 0, &queue_);
		return true;
	}

	bool VulkanWindow::createSkiaContext() {
		std::vector<const char*> instanceExtensions;
		std::vector<const char*> deviceExtensions;
		for (const std::string& name : instanceExtensionNames_) {
			instanceExtensions.push_back(name.c_str());
		}
		for (const std::string& name : deviceExtensionNames_) {
			deviceExtensions.push_back(name.c_str());
		}

		extensions_.init(&vulkanGetProc,
			instance_,
			physicalDevice_,
			static_cast<std::uint32_t>(instanceExtensions.size()),
			instanceExtensions.data(),
			static_cast<std::uint32_t>(deviceExtensions.size()),
			deviceExtensions.data());

		skgpu::VulkanBackendContext backend{};
		backend.fInstance           = instance_;
		backend.fPhysicalDevice     = physicalDevice_;
		backend.fDevice             = device_;
		backend.fQueue              = queue_;
		backend.fGraphicsQueueIndex = queueFamilyIndex_;
		backend.fMaxAPIVersion      = VK_API_VERSION_1_1;
		backend.fVkExtensions       = &extensions_;
		backend.fDeviceFeatures2    = &deviceFeatures2_;
		backend.fGetProc            = &vulkanGetProc;
		// Leave fMemoryAllocator null so Skia creates its bundled VMA allocator internally; the Skia
		// library is built with SK_USE_VMA, so MakeVulkan supplies the allocator for us.

		context_ = GrDirectContexts::MakeVulkan(backend);
		if (context_ == nullptr) {
			std::fprintf(stderr, "[example] GrDirectContexts::MakeVulkan failed\n");
			return false;
		}
		return true;
	}

	bool VulkanWindow::createSwapchain() {
		VkSurfaceCapabilitiesKHR capabilities{};
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, surface_, &capabilities);

		std::uint32_t formatCount = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, nullptr);
		if (formatCount == 0) {
			std::fprintf(stderr, "[example] no surface formats\n");
			return false;
		}
		std::vector<VkSurfaceFormatKHR> formats(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, formats.data());

		VkSurfaceFormatKHR chosen = formats[0];
		for (const VkSurfaceFormatKHR& format : formats) {
			if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
				format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				chosen = format;
				break;
			}
		}
		swapchainFormat_ = chosen.format;

		int framebufferWidth  = 0;
		int framebufferHeight = 0;
		glfwGetFramebufferSize(window_, &framebufferWidth, &framebufferHeight);

		VkExtent2D extent = capabilities.currentExtent;
		if (capabilities.currentExtent.width == UINT32_MAX) {
			extent.width  = clampU32(static_cast<std::uint32_t>(framebufferWidth),
				capabilities.minImageExtent.width,
				capabilities.maxImageExtent.width);
			extent.height = clampU32(static_cast<std::uint32_t>(framebufferHeight),
				capabilities.minImageExtent.height,
				capabilities.maxImageExtent.height);
		}
		if (extent.width == 0 || extent.height == 0) {
			return false; // minimized; the caller will retry
		}
		swapchainExtent_ = extent;

		std::uint32_t imageCount = capabilities.minImageCount + 1;
		if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
			imageCount = capabilities.maxImageCount;
		}

		VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		usage &= capabilities.supportedUsageFlags;
		swapchainUsage_ = usage;

		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface          = surface_;
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

		if (vkCreateSwapchainKHR(device_, &createInfo, nullptr, &swapchain_) != VK_SUCCESS) {
			std::fprintf(stderr, "[example] vkCreateSwapchainKHR failed\n");
			return false;
		}

		vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, nullptr);
		swapchainImages_.resize(imageCount);
		vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, swapchainImages_.data());

		const SkColorType colorType =
			swapchainFormat_ == VK_FORMAT_B8G8R8A8_UNORM ? kBGRA_8888_SkColorType : kRGBA_8888_SkColorType;

		std::fprintf(stderr,
			"[example] diag bgraSurface=%d rgbaSurface=%d maxRT=%d\n",
			context_->colorTypeSupportedAsSurface(kBGRA_8888_SkColorType) ? 1 : 0,
			context_->colorTypeSupportedAsSurface(kRGBA_8888_SkColorType) ? 1 : 0,
			context_->maxRenderTargetSize());

		surfaces_.assign(imageCount, nullptr);
		for (std::uint32_t i = 0; i < imageCount; ++i) {
			GrVkImageInfo imageInfo;
			imageInfo.fImage              = swapchainImages_[i];
			imageInfo.fImageTiling        = VK_IMAGE_TILING_OPTIMAL;
			imageInfo.fImageLayout        = VK_IMAGE_LAYOUT_UNDEFINED;
			imageInfo.fFormat             = swapchainFormat_;
			imageInfo.fImageUsageFlags    = usage;
			imageInfo.fSampleCount        = 1;
			imageInfo.fLevelCount         = 1;
			imageInfo.fCurrentQueueFamily = VK_QUEUE_FAMILY_IGNORED;
			imageInfo.fSharingMode        = VK_SHARING_MODE_EXCLUSIVE;

			GrBackendRenderTarget renderTarget = GrBackendRenderTargets::MakeVk(
				static_cast<int>(extent.width),
				static_cast<int>(extent.height),
				imageInfo);

			surfaces_[i] = SkSurfaces::WrapBackendRenderTarget(context_.get(),
				renderTarget,
				kTopLeft_GrSurfaceOrigin,
				colorType,
				nullptr,
				nullptr);
			if (surfaces_[i] == nullptr) {
				std::fprintf(stderr,
					"[example] WrapBackendRenderTarget failed (format=%d colorType=%d rtValid=%d usage=0x%x extent=%ux%u)\n",
					static_cast<int>(swapchainFormat_),
					static_cast<int>(colorType),
					renderTarget.isValid() ? 1 : 0,
					static_cast<unsigned>(usage),
					extent.width,
					extent.height);
				return false;
			}
		}
		return true;
	}

	void VulkanWindow::destroySwapchain() {
		surfaces_.clear();
		swapchainImages_.clear();
		if (swapchain_ != VK_NULL_HANDLE) {
			vkDestroySwapchainKHR(device_, swapchain_, nullptr);
			swapchain_ = VK_NULL_HANDLE;
		}
	}

	bool VulkanWindow::recreateSwapchain() {
		// Block while minimized, then rebuild against the current framebuffer size.
		int width  = 0;
		int height = 0;
		glfwGetFramebufferSize(window_, &width, &height);
		while ((width == 0 || height == 0) && glfwWindowShouldClose(window_) == GLFW_FALSE) {
			glfwWaitEvents();
			glfwGetFramebufferSize(window_, &width, &height);
		}

		if (device_ != VK_NULL_HANDLE) {
			vkDeviceWaitIdle(device_);
		}
		destroySwapchain();
		return createSwapchain();
	}

	void VulkanWindow::drawFrame(const DrawCallback& draw) {
		if (framebufferResized_) {
			framebufferResized_ = false;
			if (!recreateSwapchain()) {
				return;
			}
		}
		if (swapchain_ == VK_NULL_HANDLE) {
			recreateSwapchain();
			return;
		}

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType          = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		VkSemaphore acquireSemaphore = VK_NULL_HANDLE;
		if (vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &acquireSemaphore) != VK_SUCCESS) {
			return;
		}

		std::uint32_t  imageIndex = 0;
		const VkResult acquire    = vkAcquireNextImageKHR(
			device_,
			swapchain_,
			UINT64_MAX,
			acquireSemaphore,
			VK_NULL_HANDLE,
			&imageIndex);
		if (acquire == VK_ERROR_OUT_OF_DATE_KHR) {
			vkDestroySemaphore(device_, acquireSemaphore, nullptr);
			recreateSwapchain();
			return;
		}
		if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR) {
			vkDestroySemaphore(device_, acquireSemaphore, nullptr);
			return;
		}

		SkSurface* surface = surfaces_[imageIndex].get();

		// Have Skia's GPU work wait on the acquire semaphore; Skia takes ownership and deletes it.
		GrBackendSemaphore backendAcquire = GrBackendSemaphores::MakeVk(acquireSemaphore);
		if (!surface->wait(1, &backendAcquire)) {
			vkDestroySemaphore(device_, acquireSemaphore, nullptr);
			return;
		}

		draw(surface->getCanvas(),
			static_cast<int>(swapchainExtent_.width),
			static_cast<int>(swapchainExtent_.height),
			queryDpr());

		GrFlushInfo                flushInfo{};
		skgpu::MutableTextureState presentState =
			skgpu::MutableTextureStates::MakeVulkan(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, queueFamilyIndex_);
		context_->flush(surface, flushInfo, &presentState);
		context_->submit();

		vkQueueWaitIdle(queue_);

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType          = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains    = &swapchain_;
		presentInfo.pImageIndices  = &imageIndex;

		const VkResult present = vkQueuePresentKHR(queue_, &presentInfo);
		if (present == VK_ERROR_OUT_OF_DATE_KHR || present == VK_SUBOPTIMAL_KHR || framebufferResized_) {
			framebufferResized_ = false;
			recreateSwapchain();
		}
	}

	float VulkanWindow::queryDpr() const {
		int windowWidth       = 0;
		int windowHeight      = 0;
		int framebufferWidth  = 0;
		int framebufferHeight = 0;
		glfwGetWindowSize(window_, &windowWidth, &windowHeight);
		glfwGetFramebufferSize(window_, &framebufferWidth, &framebufferHeight);
		if (windowWidth > 0 && framebufferWidth > 0) {
			return static_cast<float>(framebufferWidth) / static_cast<float>(windowWidth);
		}
		return 1.0f;
	}

	void VulkanWindow::cleanup() {
		if (device_ != VK_NULL_HANDLE) {
			vkDeviceWaitIdle(device_);
		}
		destroySwapchain();
		context_.reset(); // release the Skia GPU context before its Vulkan device

		if (device_ != VK_NULL_HANDLE) {
			vkDestroyDevice(device_, nullptr);
			device_ = VK_NULL_HANDLE;
		}
		if (surface_ != VK_NULL_HANDLE) {
			vkDestroySurfaceKHR(instance_, surface_, nullptr);
			surface_ = VK_NULL_HANDLE;
		}
		if (instance_ != VK_NULL_HANDLE) {
			vkDestroyInstance(instance_, nullptr);
			instance_ = VK_NULL_HANDLE;
		}
		if (window_ != nullptr) {
			glfwDestroyWindow(window_);
			window_ = nullptr;
		}
		glfwTerminate();
	}
} // namespace example
