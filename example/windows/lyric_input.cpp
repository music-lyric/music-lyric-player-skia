#include "lyric_input.h"

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

// This file drives Win32 through the wide-character APIs, so select the UNICODE resource macros before windows.h.
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>

namespace example {
	namespace {
		// Identifies the multi-line edit control that receives the pasted hex text.
		constexpr int kEditControlId = 1001;

		/**
		 * Carries the prompt's result out of its window procedure and back to the modal loop.
		 */
		struct PromptState {
			HWND         edit     = nullptr;
			bool         done     = false;
			bool         accepted = false;
			std::wstring text;
		};

		/**
		 * Converts a UTF-8 string to UTF-16 for the Win32 wide-character controls.
		 */
		std::wstring utf8ToWide(const std::string& input) {
			if (input.empty()) {
				return {};
			}
			const int length = MultiByteToWideChar(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), nullptr, 0);
			std::wstring output(static_cast<std::size_t>(length), L'\0');
			MultiByteToWideChar(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), output.data(), length);
			return output;
		}

		/**
		 * Converts a UTF-16 string from the Win32 controls back to UTF-8.
		 */
		std::string wideToUtf8(const std::wstring& input) {
			if (input.empty()) {
				return {};
			}
			const int length = WideCharToMultiByte(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), nullptr, 0, nullptr, nullptr);
			std::string output(static_cast<std::size_t>(length), '\0');
			WideCharToMultiByte(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), output.data(), length, nullptr, nullptr);
			return output;
		}

		/**
		 * Reads the full text of a control into a wide string.
		 */
		std::wstring controlText(HWND control) {
			const int length = GetWindowTextLengthW(control);
			if (length <= 0) {
				return {};
			}
			std::wstring text(static_cast<std::size_t>(length), L'\0');
			GetWindowTextW(control, text.data(), length + 1);
			return text;
		}

		/**
		 * Casts a control id to the `HMENU` slot that `CreateWindowExW` uses to tag child controls.
		 */
		HMENU asControlId(int id) {
			return reinterpret_cast<HMENU>(static_cast<std::intptr_t>(id));
		}

		/**
		 * Handles commands from the prompt's buttons, recording the outcome for the modal loop.
		 */
		LRESULT CALLBACK promptProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
			auto* state = reinterpret_cast<PromptState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
			switch (message) {
				case WM_COMMAND:
					if (state != nullptr) {
						const int id = LOWORD(wParam);
						if (id == IDOK) {
							state->text     = controlText(state->edit);
							state->accepted = true;
							state->done     = true;
							return 0;
						}
						if (id == IDCANCEL) {
							state->done = true;
							return 0;
						}
					}
					break;
				case WM_CLOSE:
					if (state != nullptr) {
						state->done = true;
					}
					return 0;
				default:
					break;
			}
			return DefWindowProcW(window, message, wParam, lParam);
		}

		/**
		 * Returns the path, next to the executable, where the last loaded lyric is persisted.
		 */
		std::filesystem::path persistedLyricPath() {
			wchar_t     buffer[MAX_PATH] = {};
			const DWORD length           = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
			const std::filesystem::path executable(buffer, buffer + length);
			return executable.parent_path() / L"last_lyric.bin";
		}
	} // namespace

	std::optional<std::vector<std::uint8_t>> decodeHex(const std::string& input) {
		std::vector<std::uint8_t> bytes;
		bytes.reserve(input.size() / 2);
		int high = -1;
		for (const char c : input) {
			if (std::isspace(static_cast<unsigned char>(c)) != 0) {
				continue;
			}
			int value = 0;
			if (c >= '0' && c <= '9') {
				value = c - '0';
			} else if (c >= 'a' && c <= 'f') {
				value = c - 'a' + 10;
			} else if (c >= 'A' && c <= 'F') {
				value = c - 'A' + 10;
			} else {
				return std::nullopt;
			}
			if (high < 0) {
				high = value;
			} else {
				bytes.push_back(static_cast<std::uint8_t>((high << 4) | value));
				high = -1;
			}
		}
		if (high >= 0 || bytes.empty()) {
			return std::nullopt;
		}
		return bytes;
	}

	std::optional<std::string> promptHexLyric(void* ownerHwnd, const std::string& initialHex) {
		HWND            owner    = static_cast<HWND>(ownerHwnd);
		const HINSTANCE instance = GetModuleHandleW(nullptr);
		const wchar_t*  kClass   = L"MusicLyricPlayerHexPrompt";

		static bool registered = false;
		if (!registered) {
			WNDCLASSEXW windowClass = {};
			windowClass.cbSize        = sizeof(windowClass);
			windowClass.lpfnWndProc   = promptProc;
			windowClass.hInstance     = instance;
			windowClass.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
			windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
			windowClass.lpszClassName = kClass;
			RegisterClassExW(&windowClass);
			registered = true;
		}

		// The process is per-monitor DPI aware, so lay the prompt out from a 96-DPI base scaled to the owner's DPI.
		const UINT dpi   = owner != nullptr ? GetDpiForWindow(owner) : 96;
		const auto scale = [dpi](int value) {
			return MulDiv(value, static_cast<int>(dpi), 96);
		};

		const int width  = scale(560);
		const int height = scale(320);

		RECT ownerRect = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
		if (owner != nullptr) {
			GetWindowRect(owner, &ownerRect);
		}
		const int x = ownerRect.left + ((ownerRect.right - ownerRect.left) - width) / 2;
		const int y = ownerRect.top + ((ownerRect.bottom - ownerRect.top) - height) / 2;

		PromptState state;

		HWND window = CreateWindowExW(WS_EX_DLGMODALFRAME, kClass, L"Load lyric from hex",
		                              WS_POPUP | WS_CAPTION | WS_SYSMENU, x, y, width, height, owner, nullptr, instance, nullptr);
		if (window == nullptr) {
			return std::nullopt;
		}
		SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&state));

		NONCLIENTMETRICSW metrics = {};
		metrics.cbSize            = sizeof(metrics);
		SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0, dpi);
		const HFONT font = CreateFontIndirectW(&metrics.lfMessageFont);

		RECT      client = {};
		GetClientRect(window, &client);
		const int pad          = scale(12);
		const int gap          = scale(8);
		const int labelHeight  = scale(34);
		const int buttonWidth  = scale(88);
		const int buttonHeight = scale(28);

		HWND label = CreateWindowExW(0, L"STATIC",
		                             L"Paste a hex-encoded lyric (protobuf binary).\nCtrl+Enter to load, Esc to cancel.",
		                             WS_CHILD | WS_VISIBLE, pad, pad, client.right - 2 * pad, labelHeight, window, nullptr, instance,
		                             nullptr);

		const int editTop    = pad + labelHeight + gap;
		const int editBottom = client.bottom - pad - buttonHeight - gap;
		state.edit           = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
		                                       WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL, pad, editTop,
		                                       client.right - 2 * pad, editBottom - editTop, window, asControlId(kEditControlId),
		                                       instance, nullptr);
		SendMessageW(state.edit, EM_SETLIMITTEXT, 0, 0);
		if (!initialHex.empty()) {
			SetWindowTextW(state.edit, utf8ToWide(initialHex).c_str());
		}

		HWND okButton = CreateWindowExW(0, L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
		                                client.right - pad - 2 * buttonWidth - gap, client.bottom - pad - buttonHeight, buttonWidth,
		                                buttonHeight, window, asControlId(IDOK), instance, nullptr);
		HWND cancelButton = CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE,
		                                    client.right - pad - buttonWidth, client.bottom - pad - buttonHeight, buttonWidth,
		                                    buttonHeight, window, asControlId(IDCANCEL), instance, nullptr);

		for (const HWND control : {label, state.edit, okButton, cancelButton}) {
			SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
		}

		if (owner != nullptr) {
			EnableWindow(owner, FALSE);
		}
		ShowWindow(window, SW_SHOW);
		SetForegroundWindow(window);
		SetFocus(state.edit);
		SendMessageW(state.edit, EM_SETSEL, 0, -1);

		// A nested modal loop: Ctrl+Enter accepts, Esc cancels, and IsDialogMessage gives tab and default-button behaviour.
		MSG message = {};
		while (!state.done && GetMessageW(&message, nullptr, 0, 0) > 0) {
			if (message.message == WM_KEYDOWN) {
				if (message.wParam == VK_ESCAPE) {
					SendMessageW(window, WM_COMMAND, MAKEWPARAM(IDCANCEL, 0), 0);
					continue;
				}
				if (message.wParam == VK_RETURN && (GetKeyState(VK_CONTROL) & 0x8000) != 0) {
					SendMessageW(window, WM_COMMAND, MAKEWPARAM(IDOK, 0), 0);
					continue;
				}
			}
			if (IsDialogMessageW(window, &message) == FALSE) {
				TranslateMessage(&message);
				DispatchMessageW(&message);
			}
		}

		if (owner != nullptr) {
			EnableWindow(owner, TRUE);
			SetForegroundWindow(owner);
		}
		DestroyWindow(window);
		if (font != nullptr) {
			DeleteObject(font);
		}

		if (!state.accepted) {
			return std::nullopt;
		}
		return wideToUtf8(state.text);
	}

	void persistLyric(const std::vector<std::uint8_t>& bytes) {
		std::ofstream out(persistedLyricPath(), std::ios::binary | std::ios::trunc);
		if (out) {
			out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
		}
	}

	std::optional<std::vector<std::uint8_t>> loadPersistedLyric() {
		std::ifstream in(persistedLyricPath(), std::ios::binary);
		if (!in) {
			return std::nullopt;
		}
		std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		if (bytes.empty()) {
			return std::nullopt;
		}
		return bytes;
	}

	void reportError(void* ownerHwnd, const std::string& message) {
		MessageBoxW(static_cast<HWND>(ownerHwnd), utf8ToWide(message).c_str(), L"music-lyric-player", MB_ICONERROR | MB_OK);
	}
} // namespace example
