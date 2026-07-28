#include "state.h"

#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>

// Glaze is parsed before windows.h so none of the Win32 macros can leak into its templates.
#include "glaze/json.hpp"
// Win32 only supplies the module path lookup that locates the state file.
#include <windows.h>

// The config tree rides along in the state file, so its enum names and its sparse leaves need their glaze bindings here.
#include "rendering/config/config.gen.glaze.h"
#include "utils/config/property.glaze.h"

namespace example {
	namespace {
		/**
		 * Returns the path, next to the executable, where the session is mirrored.
		 */
		std::filesystem::path stateFile() {
			wchar_t                     buffer[MAX_PATH] = {};
			const DWORD                 length           = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
			const std::filesystem::path executable(buffer, buffer + length);
			return executable.parent_path() / L"state.json";
		}
	} // namespace

	AppState loadState() {
		std::ifstream in(stateFile(), std::ios::binary);
		if (!in) {
			return {};
		}

		const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		if (text.empty()) {
			return {};
		}

		// A malformed or older file must never stop the demo from starting, so a parse failure falls back to defaults.
		AppState state;
		if (glz::read_json(state, text)) {
			return {};
		}
		return state;
	}

	void saveState(const AppState& state) {
		std::string text;
		if (glz::write_json(state, text)) {
			return;
		}

		std::ofstream out(stateFile(), std::ios::binary | std::ios::trunc);
		if (out) {
			out.write(text.data(), static_cast<std::streamsize>(text.size()));
		}
	}
} // namespace example
