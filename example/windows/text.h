#ifndef MUSIC_LYRIC_PLAYER_EXAMPLE_TEXT_H_
#define MUSIC_LYRIC_PLAYER_EXAMPLE_TEXT_H_

#include <string>

namespace example {
	/**
	 * Converts a UTF-8 string to UTF-16 for the Win32 wide-character APIs.
	 */
	std::wstring utf8ToWide(const std::string& input);

	/**
	 * Converts a UTF-16 string from the Win32 wide-character APIs back to UTF-8.
	 */
	std::string wideToUtf8(const std::wstring& input);
} // namespace example

#endif // MUSIC_LYRIC_PLAYER_EXAMPLE_TEXT_H_
