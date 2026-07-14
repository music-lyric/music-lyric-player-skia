#include "window.h"

#include <cstdio>

#include <windows.h>

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace example {
	Window::Window() = default;

	Window::~Window() {
		if (this->window != nullptr) {
			glfwDestroyWindow(this->window);
		}
		glfwTerminate();
	}

	bool Window::init(int width, int height, const char* title) {
		// The backend surface reads the client rect in physical pixels and the window DPI directly,
		// so the process must be per-monitor DPI aware or sizes and the device-pixel ratio go wrong.
		SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

		if (glfwInit() == GLFW_FALSE) {
			std::fprintf(stderr, "[example] glfwInit failed\n");
			return false;
		}

		// No client API: the backend owns the whole Vulkan stack; GLFW only provides the window and input.
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		this->window = glfwCreateWindow(width, height, title, nullptr, nullptr);
		if (this->window == nullptr) {
			std::fprintf(stderr, "[example] glfwCreateWindow failed\n");
			return false;
		}

		glfwSetWindowUserPointer(this->window, this);
		glfwSetFramebufferSizeCallback(this->window, [](GLFWwindow* window, int, int) {
			auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
			if (self != nullptr) {
				self->resized = true;
			}
		});

		return true;
	}

	bool Window::shouldClose() const {
		return this->window == nullptr || glfwWindowShouldClose(this->window) == GLFW_TRUE;
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
} // namespace example
