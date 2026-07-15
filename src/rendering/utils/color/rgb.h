#ifndef MUSIC_LYRIC_PLAYER_RENDERING_UTILS_COLOR_RGB_H_
#define MUSIC_LYRIC_PLAYER_RENDERING_UTILS_COLOR_RGB_H_

#include <cstddef>
#include <optional>
#include <string_view>

#include "include/core/SkColor.h"
#include "rendering/utils/length.h"

namespace music_lyric_player::rendering::utils::color::detail {
	inline int clampChannel(double value) {
		if (value <= 0.0) {
			return 0;
		}
		if (value >= 255.0) {
			return 255;
		}
		return static_cast<int>(value + 0.5);
	}

	/**
	 * Parses the text inside `rgb(...)` / `rgba(...)` into ARGB.
	 * Expects three comma-separated 0..255 channels plus an optional 0..1 alpha; alpha defaults to opaque.
	 * Returns no value on the wrong component count or an unparseable component.
	 */
	inline ::std::optional<SkColor> parseRgb(::std::string_view body) {
		::std::optional<double> parts[4];
		int                     count = 0;
		::std::size_t           start = 0;
		while (true) {
			if (count >= 4) {
				return ::std::nullopt; // more than four components
			}
			const ::std::size_t      comma = body.find(',', start);
			const ::std::string_view piece = body.substr(start, comma == ::std::string_view::npos ? ::std::string_view::npos : comma - start);
			parts[count++]                 = rendering::detail::parseNumber(rendering::detail::trimLength(piece));
			if (comma == ::std::string_view::npos) {
				break;
			}
			start = comma + 1;
		}
		if (count < 3 || !parts[0].has_value() || !parts[1].has_value() || !parts[2].has_value()) {
			return ::std::nullopt;
		}
		int alpha = 255;
		if (count == 4) {
			if (!parts[3].has_value()) {
				return ::std::nullopt;
			}
			const double clamped = *parts[3] <= 0.0 ? 0.0 : (*parts[3] >= 1.0 ? 1.0 : *parts[3]);
			alpha                = static_cast<int>(clamped * 255.0 + 0.5);
		}
		return SkColorSetARGB(
			static_cast<U8CPU>(alpha),
			static_cast<U8CPU>(clampChannel(*parts[0])),
			static_cast<U8CPU>(clampChannel(*parts[1])),
			static_cast<U8CPU>(clampChannel(*parts[2])));
	}
} // namespace music_lyric_player::rendering::utils::color::detail

#endif // MUSIC_LYRIC_PLAYER_RENDERING_UTILS_COLOR_RGB_H_
