#include "surface.h"

#include <memory>

#include "backend/gpu/gles/gles.h"
#include "backend/gpu/vulkan/vulkan.h"
#include "utils/logger/logger.h"

namespace music_lyric_player::platform::android {
	namespace {
		constexpr utils::Logger logger{"AndroidSurface"};

		/**
		 * Which backend the process draws through, once a surface has been made through one.
		 */
		enum class Backend {
			Undecided,
			Vulkan,
			Gles,
		};

		Backend chosen = Backend::Undecided;
	} // namespace

	std::unique_ptr<backend::gpu::Surface> createSurface(ANativeWindow* window) {
		const backend::gpu::NativeWindow native{window};

		if (chosen != Backend::Gles) {
			std::unique_ptr<backend::gpu::Surface> surface = backend::gpu::vulkan::createSurface(native);
			if (surface != nullptr) {
				if (chosen == Backend::Undecided) {
					chosen = Backend::Vulkan;
					logger.info("drawing through Vulkan");
				}
				return surface;
			}

			// Falling back is a first surface's move alone: once the process draws through Vulkan, a window that cannot join it is a fault of that window rather than a missing driver, and answering it with a second GPU stack would leave the two halves of the app on different backends.
			if (chosen == Backend::Vulkan) {
				logger.error("Vulkan could not open a surface for this window");
				return nullptr;
			}
			logger.warn("Vulkan is unavailable on this device, falling back to GLES");
		}

		std::unique_ptr<backend::gpu::Surface> surface = backend::gpu::gles::createSurface(native);
		if (surface == nullptr) {
			logger.error("no GPU backend could open a surface for this window");
			return nullptr;
		}
		if (chosen == Backend::Undecided) {
			chosen = Backend::Gles;
			logger.info("drawing through GLES");
		}
		return surface;
	}
} // namespace music_lyric_player::platform::android
