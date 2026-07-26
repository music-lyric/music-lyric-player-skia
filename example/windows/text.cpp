#include "text.h"

#include <cstddef>

#include <windows.h>

namespace example {
	std::wstring utf8ToWide(const std::string& input) {
		if (input.empty()) {
			return {};
		}
		const int    length = MultiByteToWideChar(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), nullptr, 0);
		std::wstring output(static_cast<std::size_t>(length), L'\0');
		MultiByteToWideChar(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), output.data(), length);
		return output;
	}

	std::string wideToUtf8(const std::wstring& input) {
		if (input.empty()) {
			return {};
		}
		const int   length = WideCharToMultiByte(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), nullptr, 0, nullptr, nullptr);
		std::string output(static_cast<std::size_t>(length), '\0');
		WideCharToMultiByte(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), output.data(), length, nullptr, nullptr);
		return output;
	}
} // namespace example
