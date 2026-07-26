#ifndef MUSIC_LYRIC_PLAYER_EXAMPLE_STATE_H_
#define MUSIC_LYRIC_PLAYER_EXAMPLE_STATE_H_

#include <string>

namespace example {
	/**
	 * The audio half of the persisted session.
	 */
	struct AudioState {
		// Absolute path in UTF-8; empty when no track was ever opened.
		std::string path;
		float       volume = 1.0f;
	};

	/**
	 * The lyric half of the persisted session, kept as the hex payload the demo was handed.
	 */
	struct LyricState {
		std::string hex;
	};

	/**
	 * Everything the demo restores on its next launch, mirrored to `state.json`.
	 */
	struct AppState {
		AudioState audio;
		LyricState lyric;
	};

	/**
	 * Reads `state.json` from next to the executable, yielding defaults when it is absent or malformed.
	 */
	AppState loadState();

	/**
	 * Writes `state.json` next to the executable.
	 */
	void saveState(const AppState& state);
} // namespace example

#endif // MUSIC_LYRIC_PLAYER_EXAMPLE_STATE_H_
