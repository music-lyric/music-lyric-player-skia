#include "backend/gpu/gles/index.h"

#include <memory>
#include <utility>

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/native_window.h>

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
#include "include/gpu/ganesh/gl/GrGLTypes.h"
#include "include/gpu/ganesh/gl/egl/GrGLMakeEGLInterface.h"
#include "utils/logger/logger.h"

namespace music_lyric_player::backend::gpu::gles {
	namespace {
		constexpr utils::Logger logger{"GlesSurface"};

		// A window surface draws into the default framebuffer, which is the one Skia wraps as the backbuffer.
		constexpr GrGLuint kDefaultFramebuffer = 0;

		// The EGL config below asks for an 8 bit stencil, which Skia needs for clipping and path fills.
		constexpr int kStencilBits = 8;

		// The backbuffer is never multisampled: Skia antialiases its own geometry, so MSAA would only cost fill rate.
		constexpr int kSampleCount = 0;

		/**
		 * The EGL display, context and Skia context that every GLES surface in the process shares.
		 * One context per window would mean one Skia glyph and texture cache per window, and switching between them still costs a `makeCurrent`, so the first surface builds this and later surfaces borrow it.
		 * It is released once the last surface using it is gone, and is only touched from the thread driving them.
		 */
		class SharedContext {
		public:
			~SharedContext() {
				destroy();
			}

			/**
			 * Opens the default display and picks a config plus a GLES 2 context; returns false when EGL refuses any of it.
			 */
			bool init();

			/**
			 * Builds the Skia context against the first window surface, which is the earliest point a GL context can be made current.
			 * For a later surface the context already exists and this only binds it.
			 */
			bool completeFor(EGLSurface drawSurface);

			/**
			 * Binds the context to `drawSurface`, invalidating Skia's state cache when the binding actually had to change.
			 */
			bool makeCurrent(EGLSurface drawSurface);

			/**
			 * Forgets `drawSurface` if it is the one currently bound, so a destroyed surface is never made current again.
			 */
			void releaseCurrent(EGLSurface drawSurface);

			EGLDisplay             display = EGL_NO_DISPLAY;
			EGLConfig              config  = nullptr;
			EGLContext             context = EGL_NO_CONTEXT;
			sk_sp<GrDirectContext> grContext;

		private:
			/**
			 * Tears the stack down in reverse creation order.
			 */
			void destroy();

			EGLSurface currentDrawSurface = EGL_NO_SURFACE;
		};

		// Held weakly so the stack goes away with the last surface instead of outliving the windows.
		std::weak_ptr<SharedContext> sharedContext;

		/**
		 * Returns the process-wide GPU stack, standing it up when no surface holds one yet.
		 */
		std::shared_ptr<SharedContext> acquireSharedContext() {
			std::shared_ptr<SharedContext> shared = sharedContext.lock();
			if (shared != nullptr) {
				return shared;
			}

			shared = std::make_shared<SharedContext>();
			if (!shared->init()) {
				return nullptr;
			}
			sharedContext = shared;
			return shared;
		}

		/**
		 * An EGL window surface bound to an `ANativeWindow`, exposing its default framebuffer as a Skia surface.
		 * Owns only the EGL surface; the display, context and Skia context come from the shared stack.
		 * The window is retained for as long as the surface lives, so the host releasing its own reference first cannot leave this drawing into freed memory.
		 */
		class WindowSurface : public Surface {
		public:
			explicit WindowSurface(std::shared_ptr<SharedContext> shared) : shared(std::move(shared)) {}

			~WindowSurface() override {
				cleanup();
			}

			/**
			 * Binds the surface to `window` and stands up the shared GPU stack against it; returns false on any failure.
			 */
			bool init(ANativeWindow* window);

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
			 * Rebuilds the Skia surface against the EGL surface's current size.
			 * Leaves the surface pending when the window has no area, so the next frame tries again.
			 */
			bool rebuildSurface();

			/**
			 * Releases the Skia surface, the EGL surface and the window, in that order.
			 */
			void cleanup();

			std::shared_ptr<SharedContext> shared;
			ANativeWindow*                 window     = nullptr;
			EGLSurface                     eglSurface = EGL_NO_SURFACE;
			sk_sp<SkSurface>               surface;

			int backbufferWidth  = 0;
			int backbufferHeight = 0;
			// The window reports its own size, so a resize only has to mark the Skia surface stale.
			bool needRebuild = true;
		};

		bool SharedContext::init() {
			this->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
			if (this->display == EGL_NO_DISPLAY) {
				logger.error("no EGL display");
				return false;
			}
			if (eglInitialize(this->display, nullptr, nullptr) != EGL_TRUE) {
				logger.error("eglInitialize failed (0x%x)", eglGetError());
				return false;
			}

			// EGL_WINDOW_BIT rather than the pbuffer bit Skia's own sample picks: every surface here is a window, and the Skia context is built once the first of them exists.
			// Depth goes unmentioned because EGL reads every buffer size as a minimum, so asking for zero would not rule a depth buffer out anyway.
			const EGLint attributes[] = {
				EGL_SURFACE_TYPE,
				EGL_WINDOW_BIT,
				EGL_RENDERABLE_TYPE,
				EGL_OPENGL_ES2_BIT,
				EGL_RED_SIZE,
				8,
				EGL_GREEN_SIZE,
				8,
				EGL_BLUE_SIZE,
				8,
				EGL_ALPHA_SIZE,
				8,
				EGL_STENCIL_SIZE,
				kStencilBits,
				EGL_NONE,
			};

			EGLint configCount = 0;
			if (eglChooseConfig(this->display, attributes, &this->config, 1, &configCount) != EGL_TRUE || configCount == 0) {
				logger.error("no EGL config with an 8 bit stencil (0x%x)", eglGetError());
				return false;
			}

			const EGLint contextAttributes[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};

			this->context = eglCreateContext(this->display, this->config, EGL_NO_CONTEXT, contextAttributes);
			if (this->context == EGL_NO_CONTEXT) {
				logger.error("eglCreateContext failed (0x%x)", eglGetError());
				return false;
			}
			return true;
		}

		bool SharedContext::completeFor(EGLSurface drawSurface) {
			if (!makeCurrent(drawSurface)) {
				return false;
			}
			if (this->grContext != nullptr) {
				return true; // the stack is already built, so a later window only had to be bound
			}

			sk_sp<const GrGLInterface> glInterface = GrGLInterfaces::MakeEGL();
			if (glInterface == nullptr) {
				logger.error("GrGLInterfaces::MakeEGL failed");
				return false;
			}

			this->grContext = GrDirectContexts::MakeGL(std::move(glInterface));
			if (this->grContext == nullptr) {
				logger.error("GrDirectContexts::MakeGL failed");
				return false;
			}
			return true;
		}

		bool SharedContext::makeCurrent(EGLSurface drawSurface) {
			if (this->currentDrawSurface == drawSurface) {
				return true;
			}
			if (eglMakeCurrent(this->display, drawSurface, drawSurface, this->context) != EGL_TRUE) {
				logger.error("eglMakeCurrent failed (0x%x)", eglGetError());
				return false;
			}
			this->currentDrawSurface = drawSurface;

			// Another surface held the binding until now, so hand Skia's state cache back invalidated rather than let it skip binds it still believes are in effect.
			if (this->grContext != nullptr) {
				this->grContext->resetContext();
			}
			return true;
		}

		void SharedContext::releaseCurrent(EGLSurface drawSurface) {
			if (this->currentDrawSurface != drawSurface) {
				return;
			}
			eglMakeCurrent(this->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
			this->currentDrawSurface = EGL_NO_SURFACE;
		}

		void SharedContext::destroy() {
			// The last surface is already gone by now, so there is nothing to make current and no way to free GL objects one by one.
			// Abandoning hands that job to `eglDestroyContext`, which drops everything the context owns anyway.
			if (this->grContext != nullptr) {
				this->grContext->abandonContext();
				this->grContext.reset();
			}

			if (this->display == EGL_NO_DISPLAY) {
				return;
			}
			eglMakeCurrent(this->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

			if (this->context != EGL_NO_CONTEXT) {
				eglDestroyContext(this->display, this->context);
				this->context = EGL_NO_CONTEXT;
			}
			eglTerminate(this->display);
			this->display = EGL_NO_DISPLAY;
		}

		bool WindowSurface::init(ANativeWindow* window) {
			if (window == nullptr) {
				logger.error("a native window is required");
				return false;
			}

			// Held for the lifetime of the surface: the host owns its own reference and may drop it as soon as it has handed the window over.
			ANativeWindow_acquire(window);
			this->window = window;

			this->eglSurface = eglCreateWindowSurface(this->shared->display, this->shared->config, window, nullptr);
			if (this->eglSurface == EGL_NO_SURFACE) {
				logger.error("eglCreateWindowSurface failed (0x%x)", eglGetError());
				return false;
			}
			return this->shared->completeFor(this->eglSurface);
		}

		bool WindowSurface::rebuildSurface() {
			this->surface.reset();

			// The EGL surface, not the window, is what gets drawn into: the two disagree for a frame or two around a rotation, and wrapping the window's numbers then would tear the frame.
			EGLint width  = 0;
			EGLint height = 0;
			if (eglQuerySurface(this->shared->display, this->eglSurface, EGL_WIDTH, &width) != EGL_TRUE ||
				eglQuerySurface(this->shared->display, this->eglSurface, EGL_HEIGHT, &height) != EGL_TRUE) {
				logger.error("eglQuerySurface failed (0x%x)", eglGetError());
				return false;
			}
			this->backbufferWidth  = width;
			this->backbufferHeight = height;
			if (width <= 0 || height <= 0) {
				return false; // the window has no area yet, so there is nothing to wrap
			}

			GrGLFramebufferInfo framebuffer{};
			framebuffer.fFBOID  = kDefaultFramebuffer;
			framebuffer.fFormat = GL_RGBA8;

			GrBackendRenderTarget renderTarget = GrBackendRenderTargets::MakeGL(width, height, kSampleCount, kStencilBits, framebuffer);

			// A GL default framebuffer has its origin at the bottom left, the opposite of the Vulkan backbuffers, so taking the usual kTopLeft flips the whole frame.
			// The color space stays null, as it does on the other backends, so colors reach the compositor exactly as the renderer wrote them.
			this->surface = SkSurfaces::WrapBackendRenderTarget(this->shared->grContext.get(),
				renderTarget,
				kBottomLeft_GrSurfaceOrigin,
				kRGBA_8888_SkColorType,
				nullptr,
				nullptr);
			if (this->surface == nullptr) {
				logger.error("WrapBackendRenderTarget failed");
				return false;
			}

			this->needRebuild = false;
			return true;
		}

		void WindowSurface::renderFrame(const std::function<void(SkCanvas*)>& draw) {
			if (!this->shared->makeCurrent(this->eglSurface)) {
				return;
			}
			if (this->needRebuild && !rebuildSurface()) {
				return; // the window has no area yet; skip the frame and try again on the next one
			}

			draw(this->surface->getCanvas());

			this->shared->grContext->flushAndSubmit(this->surface.get(), GrSyncCpu::kNo);
			if (eglSwapBuffers(this->shared->display, this->eglSurface) != EGL_TRUE) {
				logger.error("eglSwapBuffers failed (0x%x)", eglGetError());
			}
		}

		void WindowSurface::handleResize(int, int) {
			// The size arguments go unused: the EGL surface follows the window on its own, so the truth is read back off it at rebuild time.
			this->needRebuild = true;
		}

		void WindowSurface::cleanup() {
			if (this->shared == nullptr) {
				return;
			}

			// Skia frees GL objects as it unwinds, which only reaches the driver while the context is current.
			if (this->surface != nullptr && this->shared->makeCurrent(this->eglSurface)) {
				this->surface.reset();
				this->shared->grContext->flushAndSubmit(GrSyncCpu::kYes);
			}
			this->surface.reset();

			if (this->eglSurface != EGL_NO_SURFACE) {
				this->shared->releaseCurrent(this->eglSurface);
				eglDestroySurface(this->shared->display, this->eglSurface);
				this->eglSurface = EGL_NO_SURFACE;
			}
			if (this->window != nullptr) {
				ANativeWindow_release(this->window);
				this->window = nullptr;
			}
		}
	} // namespace

	std::unique_ptr<Surface> createSurface(const NativeWindow& window) {
		std::shared_ptr<SharedContext> shared = acquireSharedContext();
		if (shared == nullptr) {
			return nullptr;
		}

		auto surface = std::make_unique<WindowSurface>(std::move(shared));
		if (!surface->init(static_cast<ANativeWindow*>(window.handle))) {
			return nullptr;
		}
		return surface;
	}
} // namespace music_lyric_player::backend::gpu::gles
