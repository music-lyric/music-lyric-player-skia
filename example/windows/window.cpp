#include "window.h"

#include <windows.h>

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include "utils/logger/logger.h"

namespace example {
	namespace {
		constexpr music_lyric_player::utils::Logger logger{"ExampleWindow"};

		// GLFW is process-wide, so the first window brings it up and the last one tears it down.
		int liveWindows = 0;
	} // namespace

	Window::Window() = default;

	Window::~Window() {
		if (this->window != nullptr) {
			glfwDestroyWindow(this->window);
			this->window = nullptr;
			liveWindows -= 1;
		}
		if (liveWindows == 0) {
			glfwTerminate();
		}
	}

	bool Window::init(int width, int height, const char* title) {
		// The backend surface reads the client rect in physical pixels, and the host reports the device-pixel ratio from here, so the process must be per-monitor DPI aware or both go wrong.
		if (liveWindows == 0) {
			SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

			if (glfwInit() == GLFW_FALSE) {
				logger.error("glfwInit failed");
				return false;
			}
		}

		// No client API: the backend owns the whole Vulkan stack; GLFW only provides the window and input.
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		this->window = glfwCreateWindow(width, height, title, nullptr, nullptr);
		if (this->window == nullptr) {
			logger.error("glfwCreateWindow failed");
			if (liveWindows == 0) {
				glfwTerminate();
			}
			return false;
		}
		liveWindows += 1;

		glfwSetWindowUserPointer(this->window, this);
		glfwSetFramebufferSizeCallback(this->window, [](GLFWwindow* window, int, int) {
			auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
			if (self != nullptr) {
				self->resized = true;
			}
		});

		// An input backend attached later chains onto this callback, so both it and the demo bindings see the key.
		glfwSetKeyCallback(this->window, [](GLFWwindow* window, int key, int, int action, int) {
			if (action != GLFW_PRESS && action != GLFW_REPEAT) {
				return;
			}
			auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
			if (self == nullptr) {
				return;
			}
			switch (key) {
			case GLFW_KEY_R:
				self->actions.push_back(InputAction::Restart);
				break;
			case GLFW_KEY_L:
				self->actions.push_back(InputAction::LoadHex);
				break;
			case GLFW_KEY_O:
				self->actions.push_back(InputAction::LoadAudio);
				break;
			case GLFW_KEY_P:
				self->actions.push_back(InputAction::TogglePanel);
				break;
			case GLFW_KEY_LEFT:
				self->actions.push_back(InputAction::SeekBackward);
				break;
			case GLFW_KEY_RIGHT:
				self->actions.push_back(InputAction::SeekForward);
				break;
			case GLFW_KEY_SPACE:
				self->actions.push_back(InputAction::TogglePause);
				break;
			default:
				break;
			}
		});

		return true;
	}

	bool Window::shouldClose() const {
		return this->window == nullptr || glfwWindowShouldClose(this->window) == GLFW_TRUE;
	}

	GLFWwindow* Window::handle() const {
		return this->window;
	}

	void Window::pollEvents() {
		glfwPollEvents();
	}

	void* Window::hwnd() const {
		return this->window == nullptr ? nullptr : glfwGetWin32Window(this->window);
	}

	bool Window::pollResized() {
		const bool wasResized = this->resized;
		this->resized         = false;
		return wasResized;
	}

	void Window::framebufferSize(int& width, int& height) const {
		width  = 0;
		height = 0;
		if (this->window != nullptr) {
			glfwGetFramebufferSize(this->window, &width, &height);
		}
	}

	float Window::devicePixelRatio() const {
		if (this->window == nullptr) {
			return 1.0f;
		}

		// GLFW reports the content scale as the window DPI over the platform baseline, which is exactly the ratio the renderer scales by.
		float scaleX = 1.0f;
		float scaleY = 1.0f;
		glfwGetWindowContentScale(this->window, &scaleX, &scaleY);
		return scaleX > 0.0f ? scaleX : 1.0f;
	}

	std::vector<InputAction> Window::drainActions() {
		std::vector<InputAction> drained;
		drained.swap(this->actions);
		return drained;
	}

	void Window::setTitle(const char* title) {
		if (this->window != nullptr) {
			glfwSetWindowTitle(this->window, title);
		}
	}
} // namespace example
