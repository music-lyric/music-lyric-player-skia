#ifndef MUSIC_LYRIC_PLAYER_BACKEND_GPU_VULKAN_INDEX_H_
#define MUSIC_LYRIC_PLAYER_BACKEND_GPU_VULKAN_INDEX_H_

#include <memory>

#include "backend/gpu/surface.h"

namespace music_lyric_player::backend::gpu::vulkan {
	/**
	 * Creates a window-owned Vulkan surface for `window`, or null on failure.
	 * Each surface owns its swapchain, while the Vulkan instance, device and Skia context are shared with every other live surface.
	 * The shared stack is built by the first surface and freed with the last.
	 * Surfaces are not thread safe and all of them must be created and driven from the same thread.
	 */
	std::unique_ptr<Surface> createSurface(const NativeWindow& window);
} // namespace music_lyric_player::backend::gpu::vulkan

#endif // MUSIC_LYRIC_PLAYER_BACKEND_GPU_VULKAN_INDEX_H_
