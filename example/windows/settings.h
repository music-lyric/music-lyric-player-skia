#ifndef MUSIC_LYRIC_PLAYER_EXAMPLE_SETTINGS_H_
#define MUSIC_LYRIC_PLAYER_EXAMPLE_SETTINGS_H_

#include "rendering/config/index.h"

namespace example {
	/**
	 * What the settings editor did while one frame was built.
	 */
	struct SettingsEdit {
		// A field changed a leaf, so the host merges the override store back into the renderer.
		bool changed = false;
		// An edit finished — a drag was released or a text box committed — so the host mirrors the store to disk.
		bool committed = false;
	};

	/**
	 * Draws the config editor into the current ImGui window.
	 * Fields show `current`, the renderer's resolved config, and write only the leaves they change into `overrides`.
	 * That store stays sparse, so it is both what the host merges back and what it persists.
	 */
	SettingsEdit drawSettings(const music_lyric_player::rendering::config::Root& current, music_lyric_player::rendering::config::Root& overrides);
} // namespace example

#endif // MUSIC_LYRIC_PLAYER_EXAMPLE_SETTINGS_H_
