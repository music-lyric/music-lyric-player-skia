#ifndef MUSIC_LYRIC_PLAYER_EXAMPLE_LYRIC_INPUT_H_
#define MUSIC_LYRIC_PLAYER_EXAMPLE_LYRIC_INPUT_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace example {
	/**
	 * Decodes a hex string into raw bytes, tolerating any interspersed whitespace.
	 * Returns nullopt when the input holds a non-hex character, has an odd digit count or is empty.
	 */
	std::optional<std::vector<std::uint8_t>> decodeHex(const std::string& input);

	/**
	 * Shows a modal prompt, seeded with `initialHex`, for the user to paste a hex-encoded lyric.
	 * `ownerHwnd` is the demo's native window handle, disabled for the lifetime of the prompt.
	 * Returns the entered text, or nullopt when the user cancels or closes the prompt.
	 */
	std::optional<std::string> promptHexLyric(void* ownerHwnd, const std::string& initialHex);

	/**
	 * Writes the decoded lyric payload next to the executable so the next launch can restore it.
	 */
	void persistLyric(const std::vector<std::uint8_t>& bytes);

	/**
	 * Reads the lyric payload persisted next to the executable, or nullopt when none exists.
	 */
	std::optional<std::vector<std::uint8_t>> loadPersistedLyric();

	/**
	 * Shows a modal error message owned by the demo window, used when input fails to decode.
	 */
	void reportError(void* ownerHwnd, const std::string& message);
} // namespace example

#endif // MUSIC_LYRIC_PLAYER_EXAMPLE_LYRIC_INPUT_H_
