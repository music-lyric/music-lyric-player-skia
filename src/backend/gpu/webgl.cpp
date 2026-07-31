#include "backend/gpu/webgl.h"

#include <cstdio>
#include <memory>
#include <string>
#include <utility>

#include <GLES3/gl3.h>
#include <emscripten/html5.h>
#include <emscripten/html5_webgl.h>

#include "include/core/SkCanvas.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkColorType.h"
#include "include/core/SkRefCnt.h"
#include "include/core/SkSurface.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/GrTypes.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "include/gpu/ganesh/gl/GrGLInterface.h"
#include "include/gpu/ganesh/gl/GrGLMakeWebGLInterface.h"
#include "include/gpu/ganesh/gl/GrGLTypes.h"

namespace music_lyric_player::backend::gpu {
	namespace {
		// A canvas draws into the default framebuffer, which is the one Skia wraps as the backbuffer.
		constexpr GrGLuint kDefaultFramebuffer = 0;

		// WebGL 2 backs a canvas with an 8 bit stencil, which Skia needs for clipping and path fills.
		constexpr int kStencilBits = 8;

		// The canvas is never multisampled: Skia antialiases its own geometry, so MSAA would only cost fill rate.
		constexpr int kSampleCount = 0;

		/**
		 * A Skia surface wrapping the default framebuffer of one canvas' WebGL 2 context.
		 * The canvas element itself is the single source of truth for the backbuffer size, so the surface is rebuilt from a size read back off the element rather than from anything cached here.
		 */
		class CanvasSurface : public Surface {
		public:
			~CanvasSurface() override {
				cleanup();
			}

			/**
			 * Binds the surface to the canvas `selector` matches and stands up its GPU stack.
			 * Returns false when the browser refuses a WebGL 2 context or Skia cannot drive it.
			 */
			bool init(const char* selector);

			void renderFrame(const std::function<void(SkCanvas*)>& draw) override;
			void handleResize(int width, int height) override;

			int width() const override {
				return this->backbufferWidth;
			}

			int height() const override {
				return this->backbufferHeight;
			}

		private:
			/**
			 * Points GL calls at this surface's context, invalidating Skia's state cache when the binding actually had to change.
			 */
			bool makeCurrent();

			/**
			 * Rebuilds the Skia surface against the canvas' current backbuffer size.
			 * Leaves the surface pending when the canvas has no area, so the next frame tries again.
			 */
			bool rebuildSurface();

			/**
			 * Releases the Skia surface, the GPU context and the WebGL context, in that order.
			 */
			void cleanup();

			std::string selector;

			EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context = 0;
			sk_sp<GrDirectContext>          grContext;
			sk_sp<SkSurface>                surface;

			int  backbufferWidth  = 0;
			int  backbufferHeight = 0;
			bool needRebuild      = true; // rebuild the Skia surface before the next frame
		};

		bool CanvasSurface::init(const char* selector) {
			if (selector == nullptr || *selector == '\0') {
				std::fprintf(stderr, "[backend] a canvas selector is required\n");
				return false;
			}
			this->selector = selector;

			EmscriptenWebGLContextAttributes attributes{};
			emscripten_webgl_init_context_attributes(&attributes);
			attributes.majorVersion = 2; // Ganesh's WebGL backend speaks GLES3, which only WebGL 2 provides
			attributes.minorVersion = 0;
			attributes.alpha        = true;
			// The compositor expects premultiplied alpha, matching how Skia already stores its colors.
			attributes.premultipliedAlpha = true;
			attributes.depth              = false; // a 2D renderer never depth tests
			attributes.stencil            = true;  // Skia clips and fills paths through the stencil buffer
			attributes.antialias          = false;
			// Nothing reads the canvas back between frames, and preserving it would cost a copy per frame.
			attributes.preserveDrawingBuffer = false;

			this->context = emscripten_webgl_create_context(this->selector.c_str(), &attributes);
			if (this->context <= 0) {
				std::fprintf(stderr, "[backend] no WebGL 2 context for '%s'\n", this->selector.c_str());
				return false;
			}
			if (!makeCurrent()) {
				return false;
			}

			sk_sp<const GrGLInterface> glInterface = GrGLInterfaces::MakeWebGL();
			if (glInterface == nullptr) {
				std::fprintf(stderr, "[backend] GrGLInterfaces::MakeWebGL failed\n");
				return false;
			}

			this->grContext = GrDirectContexts::MakeGL(std::move(glInterface));
			if (this->grContext == nullptr) {
				std::fprintf(stderr, "[backend] GrDirectContexts::MakeGL failed\n");
				return false;
			}
			return true;
		}

		bool CanvasSurface::makeCurrent() {
			if (emscripten_webgl_get_current_context() == this->context) {
				return true;
			}
			if (emscripten_webgl_make_context_current(this->context) != EMSCRIPTEN_RESULT_SUCCESS) {
				std::fprintf(stderr, "[backend] emscripten_webgl_make_context_current failed\n");
				return false;
			}

			// Another context held the binding until now, so hand Skia's state cache back invalidated rather than let it skip binds it still believes are in effect.
			if (this->grContext != nullptr) {
				this->grContext->resetContext();
			}
			return true;
		}

		bool CanvasSurface::rebuildSurface() {
			this->surface.reset();

			int width  = 0;
			int height = 0;
			if (emscripten_get_canvas_element_size(this->selector.c_str(), &width, &height) != EMSCRIPTEN_RESULT_SUCCESS) {
				std::fprintf(stderr, "[backend] cannot read the size of '%s'\n", this->selector.c_str());
				return false;
			}
			this->backbufferWidth  = width;
			this->backbufferHeight = height;
			if (width <= 0 || height <= 0) {
				return false; // the canvas is collapsed, so there is nothing to wrap yet
			}

			GrGLFramebufferInfo framebuffer{};
			framebuffer.fFBOID  = kDefaultFramebuffer;
			framebuffer.fFormat = GL_RGBA8;

			GrBackendRenderTarget renderTarget = GrBackendRenderTargets::MakeGL(width, height, kSampleCount, kStencilBits, framebuffer);

			// A canvas' default framebuffer has its origin at the bottom left, the opposite of the window backbuffers every other backend wraps, so taking the usual kTopLeft flips the whole frame.
			// The color space stays null, as it does on the window backends, so colors reach the compositor exactly as the renderer wrote them.
			this->surface =
				SkSurfaces::WrapBackendRenderTarget(this->grContext.get(), renderTarget, kBottomLeft_GrSurfaceOrigin, kRGBA_8888_SkColorType, nullptr, nullptr);
			if (this->surface == nullptr) {
				std::fprintf(stderr, "[backend] WrapBackendRenderTarget failed\n");
				return false;
			}

			this->needRebuild = false;
			return true;
		}

		void CanvasSurface::renderFrame(const std::function<void(SkCanvas*)>& draw) {
			if (!makeCurrent()) {
				return;
			}
			if (this->needRebuild && !rebuildSurface()) {
				return; // the canvas has no area yet; skip the frame and try again on the next one
			}

			draw(this->surface->getCanvas());

			// There is nothing to present to: the browser composites the canvas once the frame callback returns, so flushing the GPU work is all that ending a frame means here.
			this->grContext->flushAndSubmit(this->surface.get(), GrSyncCpu::kNo);
		}

		void CanvasSurface::handleResize(int width, int height) {
			if (width > 0 && height > 0) {
				// The page owns layout, so its numbers are the truth here: sizing the element from them keeps its backbuffer and the Skia surface in lockstep.
				emscripten_set_canvas_element_size(this->selector.c_str(), width, height);
			}
			this->needRebuild = true;
		}

		void CanvasSurface::cleanup() {
			if (this->context <= 0) {
				return;
			}

			// Skia frees GL objects as it unwinds, which only reaches the right driver while the context is current.
			emscripten_webgl_make_context_current(this->context);
			this->surface.reset();
			this->grContext.reset();

			emscripten_webgl_destroy_context(this->context);
			this->context = 0;
		}
	} // namespace

	std::unique_ptr<Surface> createWebglSurface(const NativeWindow& window) {
		auto surface = std::make_unique<CanvasSurface>();
		if (!surface->init(window.selector)) {
			return nullptr;
		}
		return surface;
	}
} // namespace music_lyric_player::backend::gpu
