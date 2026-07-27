#include <string>

#include <emscripten/bind.h>

namespace {
	/**
	 * Returns the version this module was built from.
	 */
	std::string version() {
		return MUSIC_LYRIC_PLAYER_VERSION;
	}
} // namespace

EMSCRIPTEN_BINDINGS(music_lyric_player) {
	emscripten::function("version", &version);
}
