#ifndef MUSIC_LYRIC_PLAYER_UTILS_CONFIG_ACCESS_H_
#define MUSIC_LYRIC_PLAYER_UTILS_CONFIG_ACCESS_H_

namespace music_lyric_player::utils::config {
	template <typename Config>
	class Manager;

	/**
	 * Passkey that gates the generated config machinery (overlay / capture / resolve).
	 * The default constructor is private and only Manager is a friend, so no other code can mint one.
	 * The implicit copy constructor stays public so a minted key can be forwarded down the recursion.
	 */
	class Access {
		Access() = default;

		template <typename Config>
		friend class Manager;
	};
} // namespace music_lyric_player::utils::config

#endif // MUSIC_LYRIC_PLAYER_UTILS_CONFIG_ACCESS_H_
