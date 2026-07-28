#include <memory>
#include <string>
#include <utility>

#include <emscripten/bind.h>

#include "backend/gpu/webgl.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPaint.h"
#include "include/core/SkRect.h"

namespace {
	/**
	 * Returns the version this module was built from.
	 */
	std::string version() {
		return MUSIC_LYRIC_PLAYER_VERSION;
	}

	/**
	 * The page's handle on one canvas, owning the GPU surface every frame is drawn through.
	 * A frame is currently a test pattern; the player and the lyric renderer plug in behind it later.
	 */
	class Renderer {
	public:
		/**
		 * Creates a renderer over the canvas `selector` matches, or null when the GPU stack cannot start.
		 * The caller owns the result and releases it through the binding's `delete()`.
		 */
		static Renderer* create(const std::string& selector) {
			music_lyric_player::backend::gpu::NativeWindow window;
			window.selector = selector.c_str();

			std::unique_ptr<music_lyric_player::backend::gpu::CanvasSurface> surface = music_lyric_player::backend::gpu::createCanvasSurface(window);
			if (surface == nullptr) {
				return nullptr;
			}
			return new Renderer(std::move(surface));
		}

		/**
		 * Resizes the canvas backbuffer to `width` by `height` physical pixels, at `devicePixelRatio`.
		 */
		void configure(int width, int height, float devicePixelRatio) {
			this->surface->configure(width, height, devicePixelRatio);
		}

		/**
		 * Draws one frame into the canvas.
		 */
		void renderFrame() {
			this->surface->renderFrame([](SkCanvas* canvas) {
				canvas->clear(SkColorSetARGB(0xFF, 0x10, 0x12, 0x18));

				// Anchored to the top left corner so a flipped surface origin is visible at a glance,
				// which a solid fill would hide.
				SkPaint paint;
				paint.setColor(SkColorSetARGB(0xFF, 0x4C, 0x8D, 0xFF));

				const SkImageInfo info = canvas->imageInfo();
				canvas->drawRect(SkRect::MakeWH(info.width() * 0.25f, info.height() * 0.25f), paint);
			});
		}

	private:
		explicit Renderer(std::unique_ptr<music_lyric_player::backend::gpu::CanvasSurface> surface) : surface(std::move(surface)) {}

		std::unique_ptr<music_lyric_player::backend::gpu::CanvasSurface> surface;
	};
} // namespace

EMSCRIPTEN_BINDINGS(music_lyric_player) {
	emscripten::class_<Renderer>("Renderer")
		.class_function("create", &Renderer::create, emscripten::allow_raw_pointers())
		.function("configure", &Renderer::configure)
		.function("renderFrame", &Renderer::renderFrame);

	emscripten::function("version", &version);
}
