#ifndef MUSIC_LYRIC_PLAYER_EXAMPLE_TEXT_H_
#define MUSIC_LYRIC_PLAYER_EXAMPLE_TEXT_H_

#include <string>

namespace example {
	/**
	 * Declares this process's console output as UTF-8, which is the encoding the logger and every path string here already carry.
	 * A console decodes what it is handed with its own output code page, and the system default turns each UTF-8 sequence into mojibake; without a console attached this does nothing.
	 */
	void useUtf8Console();

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
