#ifndef MUSIC_LYRIC_PLAYER_RENDER_UTILS_COLOR_HEX_H_
#define MUSIC_LYRIC_PLAYER_RENDER_UTILS_COLOR_HEX_H_

#include <optional>
#include <string_view>

#include "include/core/SkColor.h"

namespace music_lyric_player::render::utils::color::detail {
	inline ::std::optional<int> hexDigit(char c) {
		if (c >= '0' && c <= '9') {
			return c - '0';
		}
		if (c >= 'a' && c <= 'f') {
			return c - 'a' + 10;
		}
		if (c >= 'A' && c <= 'F') {
			return c - 'A' + 10;
		}
		return ::std::nullopt;
	}

	inline ::std::optional<int> hexByte(char hi, char lo) {
		const ::std::optional<int> high = hexDigit(hi);
		const ::std::optional<int> low  = hexDigit(lo);
		if (!high.has_value() || !low.has_value()) {
			return ::std::nullopt;
		}
		return (*high << 4) | *low;
	}

	inline ::std::optional<int> hexNibble(char c) {
		const ::std::optional<int> value = hexDigit(c);
		return value.has_value() ? ::std::optional<int>((*value << 4) | *value) : ::std::nullopt;
	}

	/**
	 * Parses a `#`-less hex colour body into ARGB, following the CSS channel order (alpha last).
	 * `RGB` / `RGBA` expand each nibble (`abc` -> `aabbcc`); `RRGGBB` / `RRGGBBAA` are read byte-wise; alpha defaults to opaque.
	 * Returns no value on an unsupported length or an invalid digit.
	 */
	inline ::std::optional<SkColor> parseHex(::std::string_view body) {
		::std::optional<int> r;
		::std::optional<int> g;
		::std::optional<int> b;
		::std::optional<int> a = 255;
		switch (body.size()) {
		case 4:
			a = hexNibble(body[3]);
			[[fallthrough]];
		case 3:
			r = hexNibble(body[0]);
			g = hexNibble(body[1]);
			b = hexNibble(body[2]);
			break;
		case 8:
			a = hexByte(body[6], body[7]);
			[[fallthrough]];
		case 6:
			r = hexByte(body[0], body[1]);
			g = hexByte(body[2], body[3]);
			b = hexByte(body[4], body[5]);
			break;
		default:
			return ::std::nullopt;
		}
		if (!r.has_value() || !g.has_value() || !b.has_value() || !a.has_value()) {
			return ::std::nullopt;
		}
		return SkColorSetARGB(static_cast<U8CPU>(*a), static_cast<U8CPU>(*r), static_cast<U8CPU>(*g), static_cast<U8CPU>(*b));
	}
} // namespace music_lyric_player::render::utils::color::detail

#endif // MUSIC_LYRIC_PLAYER_RENDER_UTILS_COLOR_HEX_H_
