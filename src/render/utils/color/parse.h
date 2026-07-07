#ifndef MUSIC_LYRIC_PLAYER_RENDER_UTILS_COLOR_PARSE_H_
#define MUSIC_LYRIC_PLAYER_RENDER_UTILS_COLOR_PARSE_H_

#include <cstddef>
#include <string>
#include <string_view>

#include "include/core/SkColor.h"
#include "render/utils/color/hex.h"
#include "render/utils/color/rgb.h"
#include "render/utils/length.h"

namespace music_lyric_player::render::utils::color {
	namespace detail {
		inline bool startsWithIgnoreCase(::std::string_view token, ::std::string_view prefix) {
			if (token.size() < prefix.size()) {
				return false;
			}
			for (::std::size_t i = 0; i < prefix.size(); ++i) {
				char c = token[i];
				if (c >= 'A' && c <= 'Z') {
					c = static_cast<char>(c - 'A' + 'a');
				}
				if (c != prefix[i]) {
					return false;
				}
			}
			return true;
		}
	} // namespace detail

	/**
	 * Resolves a CSS-style colour string into a packed ARGB `SkColor`.
	 * Accepts `#RGB` / `#RGBA` / `#RRGGBB` / `#RRGGBBAA`, `rgb(r, g, b)` with 0..255 channels, or `rgba(r, g, b, a)` with `a` in 0..1.
	 * Any parse failure falls back to `fallback`.
	 */
	inline SkColor resolve(const ::std::string& value, SkColor fallback) {
		const ::std::string_view token = render::detail::trimLength(value);
		if (token.empty()) {
			return fallback;
		}

		if (token.front() == '#') {
			return detail::parseHex(token.substr(1)).value_or(fallback);
		}

		// Functional rgb() / rgba() share one grammar; the alpha is inferred from the component count.
		if (detail::startsWithIgnoreCase(token, "rgb")) {
			const ::std::size_t open = token.find('(');
			if (open == ::std::string_view::npos || token.back() != ')') {
				return fallback;
			}
			const ::std::string_view body = token.substr(open + 1, token.size() - open - 2);
			return detail::parseRgb(body).value_or(fallback);
		}

		return fallback;
	}
} // namespace music_lyric_player::render::utils::color

#endif // MUSIC_LYRIC_PLAYER_RENDER_UTILS_COLOR_PARSE_H_
