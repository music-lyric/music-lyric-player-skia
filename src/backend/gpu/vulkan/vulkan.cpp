#include "backend/gpu/vulkan/vulkan.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <vulkan/vulkan.h>

#include "backend/gpu/vulkan/platform.h"
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
#include "utils/logger/logger.h"

namespace music_lyric_player::backend::gpu::vulkan {
	namespace {
		constexpr utils::Logger logger{"VulkanSurface"};

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
		 * The Vulkan instance, logical device and Skia context that every window surface in the process shares.
		 * A second instance and device per window makes the driver stand up a whole extra GPU stack, which several drivers handle badly, so the first surface builds this one and later surfaces borrow it.
		 * Sharing one `GrDirectContext` also means the windows share Skia's glyph and texture caches.
		 * It is released once the last surface using it is gone, and is only touched from the thread driving them.
		 */
		class SharedContext {
		public:
			~SharedContext() {
				destroy();
			}

			bool createInstance();

			/**
			 * Builds the device and the Skia context against the first window surface.
			 * For a later surface the stack already exists, so this only checks it can present to that window.
			 */
			bool completeFor(VkSurfaceKHR surface);

			VkInstance             instance         = VK_NULL_HANDLE;
			VkPhysicalDevice       physicalDevice   = VK_NULL_HANDLE;
			VkDevice               device           = VK_NULL_HANDLE;
			std::uint32_t          queueFamilyIndex = 0;
			VkQueue                queue            = VK_NULL_HANDLE;
			sk_sp<GrDirectContext> context;

		private:
			/**
			 * Picks the first physical device whose queue family supports graphics and presentation to `surface`.
			 */
			bool pickPhysicalDevice(VkSurfaceKHR surface);

			/**
			 * Creates the logical device, the graphics/present queue and the swapchain extension.
			 */
			bool createDevice();

			bool createSkiaContext();

			/**
			 * Tears the stack down in reverse creation order.
			 */
			void destroy();

			VkPhysicalDeviceFeatures2 deviceFeatures2{}; // queried device features, handed to Skia

			skgpu::VulkanExtensions extensions;

			std::vector<std::string> instanceExtensionNames;
			std::vector<std::string> deviceExtensionNames;
		};

		// Held weakly so the stack goes away with the last surface instead of outliving the windows.
		std::weak_ptr<SharedContext> sharedContext;

		/**
		 * Returns the process-wide GPU stack, standing its instance up when no surface holds one yet.
		 */
		std::shared_ptr<SharedContext> acquireSharedContext() {
			std::shared_ptr<SharedContext> shared = sharedContext.lock();
			if (shared != nullptr) {
				return shared;
			}

			shared = std::make_shared<SharedContext>();
			if (!shared->createInstance()) {
				return nullptr;
			}
			sharedContext = shared;
			return shared;
		}

		/**
		 * A Vulkan swapchain bound to a platform window, exposing each backbuffer as a Skia surface.
		 * Owns only the presentation objects; the instance, device and Skia context come from the shared stack.
		 * Frames pipeline: each one waits on its own acquire semaphore and presents on its backbuffer's render semaphore, and nothing on the CPU side ever waits for the GPU.
		 * What keeps that from running away is `vkAcquireNextImageKHR`, which blocks once every backbuffer is spoken for.
		 */
		class SwapchainSurface : public Surface {
		public:
			explicit SwapchainSurface(std::shared_ptr<SharedContext> shared) : shared(std::move(shared)) {}

			/**
			 * Binds the surface to the platform window `handle` and builds its swapchain; returns false on any failure.
			 */
			bool init(void* handle);

			~SwapchainSurface() override {
				cleanup();
			}

			void renderFrame(const std::function<void(SkCanvas*)>& draw) override;
			void handleResize(int width, int height) override;

			int width() const override {
				return static_cast<int>(this->swapchainExtent.width);
			}

			int height() const override {
				return static_cast<int>(this->swapchainExtent.height);
			}

		private:
			/**
			 * Acquires the next swapchain image and returns its canvas, or null when the frame must be skipped.
			 */
			SkCanvas* acquire();

			/**
			 * Flushes and presents the frame that the last `acquire` returned a canvas for.
			 */
			void present();

			bool createSurface();

			/**
			 * Creates the swapchain against the window's current size and wraps each image as an SkSurface.
			 */
			bool createSwapchain();

			/**
			 * Destroys the swapchain along with the Skia surfaces wrapping its images.
			 */
			void destroySwapchain();

			/**
			 * Waits for the device to idle, then rebuilds the swapchain; used on resize and on an out-of-date swapchain.
			 */
			bool recreateSwapchain();

			/**
			 * Destroys the presentation objects; the shared stack outlives this and is left alone.
			 */
			void cleanup();

			std::shared_ptr<SharedContext> shared;
			void*                          window  = nullptr;
			VkSurfaceKHR                   surface = VK_NULL_HANDLE;

			VkSwapchainKHR                swapchain       = VK_NULL_HANDLE;
			VkFormat                      swapchainFormat = VK_FORMAT_UNDEFINED;
			VkExtent2D                    swapchainExtent = {0, 0};
			std::vector<VkImage>          swapchainImages;
			std::vector<sk_sp<SkSurface>> surfaces;
			std::vector<VkSemaphore>      renderSemaphores; // one per backbuffer, signalled by its frame's GPU work and waited on by its present

			int requestedWidth  = 0; // host's view of the client size, superseded by whatever the surface reports
			int requestedHeight = 0;

			bool          needRecreate      = false; // rebuild the swapchain before the next frame
			std::uint32_t currentImageIndex = 0;     // backbuffer index acquire selected, presented by present
			SkSurface*    currentSurface    = nullptr;
		};

		bool SharedContext::createInstance() {
			this->instanceExtensionNames = {
				VK_KHR_SURFACE_EXTENSION_NAME,
				surfaceExtension(),
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
				logger.error("vkCreateInstance failed");
				return false;
			}
			return true;
		}

		bool SharedContext::completeFor(VkSurfaceKHR surface) {
			if (this->context != nullptr) {
				// The stack is already built, so a later window only has to be presentable on the queue it settled on.
				VkBool32 supported = VK_FALSE;
				vkGetPhysicalDeviceSurfaceSupportKHR(this->physicalDevice, this->queueFamilyIndex, surface, &supported);
				if (supported != VK_TRUE) {
					logger.error("the shared queue family cannot present to this window");
					return false;
				}
				return true;
			}
			return pickPhysicalDevice(surface) && createDevice() && createSkiaContext();
		}

		bool SharedContext::pickPhysicalDevice(VkSurfaceKHR surface) {
			std::uint32_t deviceCount = 0;
			vkEnumeratePhysicalDevices(this->instance, &deviceCount, nullptr);
			if (deviceCount == 0) {
				logger.error("no Vulkan physical devices found");
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
					vkGetPhysicalDeviceSurfaceSupportKHR(candidate, i, surface, &presentSupport);
					const bool graphics = (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
					if (graphics && presentSupport == VK_TRUE) {
						this->physicalDevice   = candidate;
						this->queueFamilyIndex = i;
						return true;
					}
				}
			}
			logger.error("no graphics+present queue family found");
			return false;
		}

		bool SharedContext::createDevice() {
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

			// Enable every device-supported feature through a features2 chain, the same struct Skia takes as fDeviceFeatures2.
			// pEnabledFeatures must stay null while pNext carries features2.
			this->deviceFeatures2       = VkPhysicalDeviceFeatures2{};
			this->deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

			// Android's stub library only exports the Vulkan 1.1 entry points from API 28, while this module targets 24, so they are asked of the loader instead.
			// An implementation without 1.1 would already have refused the instance above, so anything reaching here carries this one.
			const auto getFeatures2 = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(vkGetInstanceProcAddr(this->instance, "vkGetPhysicalDeviceFeatures2"));
			if (getFeatures2 == nullptr) {
				logger.error("vkGetPhysicalDeviceFeatures2 is missing from a Vulkan 1.1 instance");
				return false;
			}
			getFeatures2(this->physicalDevice, &this->deviceFeatures2);

			VkDeviceCreateInfo createInfo{};
			createInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
			createInfo.pNext                   = &this->deviceFeatures2;
			createInfo.queueCreateInfoCount    = 1;
			createInfo.pQueueCreateInfos       = &queueInfo;
			createInfo.enabledExtensionCount   = static_cast<std::uint32_t>(enabled.size());
			createInfo.ppEnabledExtensionNames = enabled.data();

			if (vkCreateDevice(this->physicalDevice, &createInfo, nullptr, &this->device) != VK_SUCCESS) {
				logger.error("vkCreateDevice failed");
				return false;
			}
			vkGetDeviceQueue(this->device, this->queueFamilyIndex, 0, &this->queue);
			return true;
		}

		bool SharedContext::createSkiaContext() {
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
				logger.error("GrDirectContexts::MakeVulkan failed");
				return false;
			}
			return true;
		}

		void SharedContext::destroy() {
			if (this->device != VK_NULL_HANDLE) {
				vkDeviceWaitIdle(this->device);
			}
			this->context.reset(); // release the Skia GPU context before its Vulkan device

			if (this->device != VK_NULL_HANDLE) {
				vkDestroyDevice(this->device, nullptr);
				this->device = VK_NULL_HANDLE;
			}
			if (this->instance != VK_NULL_HANDLE) {
				vkDestroyInstance(this->instance, nullptr);
				this->instance = VK_NULL_HANDLE;
			}
		}

		bool SwapchainSurface::init(void* handle) {
			if (handle == nullptr) {
				logger.error("a native window is required");
				return false;
			}

			retainWindow(handle);
			this->window = handle;

			return createSurface() && this->shared->completeFor(this->surface) && createSwapchain();
		}

		bool SwapchainSurface::createSurface() {
			this->surface = createWindowSurface(this->shared->instance, this->window);
			return this->surface != VK_NULL_HANDLE;
		}

		bool SwapchainSurface::createSwapchain() {
			VkSurfaceCapabilitiesKHR capabilities{};
			vkGetPhysicalDeviceSurfaceCapabilitiesKHR(this->shared->physicalDevice, this->surface, &capabilities);

			std::uint32_t formatCount = 0;
			vkGetPhysicalDeviceSurfaceFormatsKHR(this->shared->physicalDevice, this->surface, &formatCount, nullptr);
			if (formatCount == 0) {
				logger.error("no surface formats");
				return false;
			}
			std::vector<VkSurfaceFormatKHR> formats(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(this->shared->physicalDevice, this->surface, &formatCount, formats.data());

			VkSurfaceFormatKHR chosen = formats[0];
			for (const VkSurfaceFormatKHR& format : formats) {
				if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
					chosen = format;
					break;
				}
			}
			this->swapchainFormat = chosen.format;

			int clientWidth  = this->requestedWidth;
			int clientHeight = this->requestedHeight;
			// Nothing has reported a size yet, so the first swapchain falls back to asking the window itself.
			if (clientWidth <= 0 || clientHeight <= 0) {
				queryWindowSize(this->window, clientWidth, clientHeight);
			}

			VkExtent2D extent = capabilities.currentExtent;
			if (capabilities.currentExtent.width == UINT32_MAX) {
				extent.width  = std::clamp(static_cast<std::uint32_t>(clientWidth), capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
				extent.height = std::clamp(static_cast<std::uint32_t>(clientHeight), capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
			}
			if (extent.width == 0 || extent.height == 0) {
				return false; // minimized; acquire skips the frame until the window is restored
			}
			this->swapchainExtent = extent;

			std::uint32_t imageCount = capabilities.minImageCount + 1;
			if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
				imageCount = capabilities.maxImageCount;
			}

			VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			usage &= capabilities.supportedUsageFlags;

			// Declaring the current transform would promise that the frame is drawn already rotated, which is a promise a 2D renderer laying out from the top left cannot keep.
			// Asking for the identity instead leaves the rotation to the compositor, at the cost of one extra pass on the platforms where a window is ever rotated at all.
			const bool rotatesItself = (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) == 0;

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
			createInfo.preTransform     = rotatesItself ? capabilities.currentTransform : VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
			createInfo.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
			createInfo.presentMode      = VK_PRESENT_MODE_FIFO_KHR; // always supported
			createInfo.clipped          = VK_TRUE;
			createInfo.oldSwapchain     = VK_NULL_HANDLE;

			if (vkCreateSwapchainKHR(this->shared->device, &createInfo, nullptr, &this->swapchain) != VK_SUCCESS) {
				logger.error("vkCreateSwapchainKHR failed");
				return false;
			}

			vkGetSwapchainImagesKHR(this->shared->device, this->swapchain, &imageCount, nullptr);
			this->swapchainImages.resize(imageCount);
			vkGetSwapchainImagesKHR(this->shared->device, this->swapchain, &imageCount, this->swapchainImages.data());

			const SkColorType colorType = this->swapchainFormat == VK_FORMAT_B8G8R8A8_UNORM ? kBGRA_8888_SkColorType : kRGBA_8888_SkColorType;

			this->surfaces.assign(imageCount, nullptr);
			this->renderSemaphores.assign(imageCount, VK_NULL_HANDLE);

			VkSemaphoreCreateInfo semaphoreInfo{};
			semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

			for (std::uint32_t i = 0; i < imageCount; ++i) {
				// One render semaphore per backbuffer rather than per frame in flight: an image cannot come back around until the present waiting on its semaphore has gone through, so a single one per image can never be signalled twice over.
				if (vkCreateSemaphore(this->shared->device, &semaphoreInfo, nullptr, &this->renderSemaphores[i]) != VK_SUCCESS) {
					logger.error("vkCreateSemaphore failed");
					return false;
				}

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

				GrBackendRenderTarget renderTarget = GrBackendRenderTargets::MakeVk(static_cast<int>(extent.width), static_cast<int>(extent.height), imageInfo);

				this->surfaces[i] =
					SkSurfaces::WrapBackendRenderTarget(this->shared->context.get(), renderTarget, kTopLeft_GrSurfaceOrigin, colorType, nullptr, nullptr);
				if (this->surfaces[i] == nullptr) {
					logger.error("WrapBackendRenderTarget failed");
					return false;
				}
			}
			return true;
		}

		void SwapchainSurface::destroySwapchain() {
			this->surfaces.clear();
			this->swapchainImages.clear();

			// Both callers wait for the device to go idle first, which is what retires the presents still holding these.
			for (VkSemaphore semaphore : this->renderSemaphores) {
				if (semaphore != VK_NULL_HANDLE) {
					vkDestroySemaphore(this->shared->device, semaphore, nullptr);
				}
			}
			this->renderSemaphores.clear();

			if (this->swapchain != VK_NULL_HANDLE) {
				vkDestroySwapchainKHR(this->shared->device, this->swapchain, nullptr);
				this->swapchain = VK_NULL_HANDLE;
			}
		}

		bool SwapchainSurface::recreateSwapchain() {
			if (this->shared->device != VK_NULL_HANDLE) {
				vkDeviceWaitIdle(this->shared->device);
			}
			destroySwapchain();
			return createSwapchain();
		}

		void SwapchainSurface::renderFrame(const std::function<void(SkCanvas*)>& draw) {
			SkCanvas* canvas = acquire();
			if (canvas == nullptr) {
				return; // minimized or swapchain out of date; skip this frame
			}
			draw(canvas);
			present();
		}

		SkCanvas* SwapchainSurface::acquire() {
			if (this->needRecreate || this->swapchain == VK_NULL_HANDLE) {
				this->needRecreate = false;
				if (!recreateSwapchain()) {
					return nullptr; // minimized or failed
				}
			}

			VkSemaphoreCreateInfo semaphoreInfo{};
			semaphoreInfo.sType          = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
			VkSemaphore acquireSemaphore = VK_NULL_HANDLE;
			if (vkCreateSemaphore(this->shared->device, &semaphoreInfo, nullptr, &acquireSemaphore) != VK_SUCCESS) {
				return nullptr;
			}

			std::uint32_t  imageIndex = 0;
			const VkResult result     = vkAcquireNextImageKHR(this->shared->device, this->swapchain, UINT64_MAX, acquireSemaphore, VK_NULL_HANDLE, &imageIndex);
			if (result == VK_ERROR_OUT_OF_DATE_KHR) {
				vkDestroySemaphore(this->shared->device, acquireSemaphore, nullptr);
				this->needRecreate = true;
				return nullptr;
			}
			if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
				vkDestroySemaphore(this->shared->device, acquireSemaphore, nullptr);
				return nullptr;
			}

			SkSurface* target = this->surfaces[imageIndex].get();

			// Have Skia's GPU work wait on the acquire semaphore; Skia takes ownership and deletes it.
			GrBackendSemaphore backendAcquire = GrBackendSemaphores::MakeVk(acquireSemaphore);
			if (!target->wait(1, &backendAcquire)) {
				vkDestroySemaphore(this->shared->device, acquireSemaphore, nullptr);
				return nullptr;
			}

			this->currentImageIndex = imageIndex;
			this->currentSurface    = target;
			return target->getCanvas();
		}

		void SwapchainSurface::present() {
			// Skia signals this once the frame's GPU work is done, which is what the present waits on instead of the CPU waiting for the queue to drain.
			VkSemaphore        renderSemaphore = this->renderSemaphores[this->currentImageIndex];
			GrBackendSemaphore backendRender   = GrBackendSemaphores::MakeVk(renderSemaphore);

			GrFlushInfo flushInfo{};
			flushInfo.fNumSemaphores    = 1;
			flushInfo.fSignalSemaphores = &backendRender;

			skgpu::MutableTextureState  presentState = skgpu::MutableTextureStates::MakeVulkan(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, this->shared->queueFamilyIndex);
			const GrSemaphoresSubmitted submitted    = this->shared->context->flush(this->currentSurface, flushInfo, &presentState);
			this->shared->context->submit();

			VkPresentInfoKHR presentInfo{};
			presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			// Presenting without the wait races the renderer, but waiting on a semaphore Skia declined to signal hangs the queue outright, so the wait only goes in when the signal was taken.
			if (submitted == GrSemaphoresSubmitted::kYes) {
				presentInfo.waitSemaphoreCount = 1;
				presentInfo.pWaitSemaphores    = &renderSemaphore;
			}
			presentInfo.swapchainCount = 1;
			presentInfo.pSwapchains    = &this->swapchain;
			presentInfo.pImageIndices  = &this->currentImageIndex;

			const VkResult result = vkQueuePresentKHR(this->shared->queue, &presentInfo);
			if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
				this->needRecreate = true;
			}
			this->currentSurface = nullptr;
		}

		void SwapchainSurface::handleResize(int width, int height) {
			// Win32 reports the client size back through the surface capabilities, so these numbers only stand in until the next swapchain asks for them.
			this->requestedWidth  = width;
			this->requestedHeight = height;
			this->needRecreate    = true;
		}

		void SwapchainSurface::cleanup() {
			if (this->shared == nullptr) {
				return;
			}

			if (this->shared->device != VK_NULL_HANDLE) {
				vkDeviceWaitIdle(this->shared->device);
			}
			destroySwapchain();

			if (this->surface != VK_NULL_HANDLE) {
				vkDestroySurfaceKHR(this->shared->instance, this->surface, nullptr);
				this->surface = VK_NULL_HANDLE;
			}
			if (this->window != nullptr) {
				releaseWindow(this->window);
				this->window = nullptr;
			}
		}

	} // namespace

	std::unique_ptr<Surface> createSurface(const NativeWindow& window) {
		std::shared_ptr<SharedContext> shared = acquireSharedContext();
		if (shared == nullptr) {
			return nullptr;
		}

		auto surface = std::make_unique<SwapchainSurface>(std::move(shared));
		if (!surface->init(window.handle)) {
			return nullptr;
		}
		return surface;
	}
} // namespace music_lyric_player::backend::gpu::vulkan
