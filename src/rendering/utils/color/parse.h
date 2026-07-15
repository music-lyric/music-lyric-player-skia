#ifndef MUSIC_LYRIC_PLAYER_RENDERING_UTILS_COLOR_PARSE_H_
#define MUSIC_LYRIC_PLAYER_RENDERING_UTILS_COLOR_PARSE_H_

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "include/core/SkColor.h"
#include "rendering/utils/color/hex.h"
#include "rendering/utils/color/rgb.h"
#include "rendering/utils/length.h"

namespace music_lyric_player::rendering::utils::color {
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

		/**
		 * Parses a CSS-style colour string into a packed ARGB `SkColor`, or no value on any failure.
		 * Accepts `#RGB` / `#RGBA` / `#RRGGBB` / `#RRGGBBAA`, `rgb(r, g, b)` with 0..255 channels, or `rgba(r, g, b, a)` with `a` in 0..1.
		 */
		inline ::std::optional<SkColor> parse(::std::string_view value) {
			const ::std::string_view token = rendering::detail::trimLength(value);
			if (token.empty()) {
				return ::std::nullopt;
			}

			if (token.front() == '#') {
				return parseHex(token.substr(1));
			}

			// Functional rgb() / rgba() share one grammar; the alpha is inferred from the component count.
			if (startsWithIgnoreCase(token, "rgb")) {
				const ::std::size_t open = token.find('(');
				if (open == ::std::string_view::npos || token.back() != ')') {
					return ::std::nullopt;
				}
				return parseRgb(token.substr(open + 1, token.size() - open - 2));
			}

			return ::std::nullopt;
		}
	} // namespace detail

	/**
	 * Resolves a CSS-style colour string into a packed ARGB `SkColor`.
	 * Accepts `#RGB` / `#RGBA` / `#RRGGBB` / `#RRGGBBAA`, `rgb(r, g, b)` with 0..255 channels, or `rgba(r, g, b, a)` with `a` in 0..1.
	 * On failure, resolves `fallback` the same way — typically the config default; if that also fails, returns transparent.
	 */
	inline SkColor resolve(const ::std::string& value, const ::std::string& fallback) {
		if (const ::std::optional<SkColor> parsed = detail::parse(value)) {
			return *parsed;
		}
		if (const ::std::optional<SkColor> parsed = detail::parse(fallback)) {
			return *parsed;
		}
		return SK_ColorTRANSPARENT;
	}

	/**
	 * Scales a color's alpha channel by an opacity in `[0, 1]`, mirroring the CSS color-times-opacity model.
	 * An opacity of `1` leaves the color unchanged; `0` makes it fully transparent.
	 */
	inline SkColor withOpacity(SkColor color, double opacity) {
		const double scale = opacity < 0.0 ? 0.0 : opacity > 1.0 ? 1.0 : opacity;
		const U8CPU  alpha = static_cast<U8CPU>(SkColorGetA(color) * scale + 0.5);
		return SkColorSetA(color, alpha);
	}
} // namespace music_lyric_player::rendering::utils::color

#endif // MUSIC_LYRIC_PLAYER_RENDERING_UTILS_COLOR_PARSE_H_
