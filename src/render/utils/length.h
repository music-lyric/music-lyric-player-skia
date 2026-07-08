#ifndef MUSIC_LYRIC_PLAYER_RENDER_UTILS_LENGTH_H_
#define MUSIC_LYRIC_PLAYER_RENDER_UTILS_LENGTH_H_

#include <cmath>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>

namespace music_lyric_player::render {
	namespace detail {
		/**
		 * Characters treated as surrounding whitespace when trimming a length token.
		 */
		inline constexpr ::std::string_view kLengthSpace = " \t\n\r\f\v";

		/**
		 * Trims leading and trailing whitespace, returning a view into the original string.
		 */
		inline ::std::string_view trimLength(::std::string_view text) {
			const ::std::size_t begin = text.find_first_not_of(kLengthSpace);
			if (begin == ::std::string_view::npos) {
				return {};
			}
			const ::std::size_t end = text.find_last_not_of(kLengthSpace);
			return text.substr(begin, end - begin + 1);
		}

		/**
		 * Parses a trimmed token as a finite decimal, requiring the whole token to be consumed.
		 * Returns no value on empty input, trailing garbage, or a non-finite result.
		 */
		inline ::std::optional<double> parseNumber(::std::string_view token) {
			if (token.empty()) {
				return ::std::nullopt;
			}
			const ::std::string buffer(token);
			const char*         begin = buffer.c_str();
			char*               end   = nullptr;
			const double        value = ::std::strtod(begin, &end);
			if (end == begin || *end != '\0' || !::std::isfinite(value)) {
				return ::std::nullopt;
			}
			return value;
		}

		/**
		 * Parses a length token into logical pixels, mirroring `resolveLength`'s grammar but without a fallback.
		 * A bare number or `px` is absolute; a `%` scales `base`, the resolved value of the inherited parent.
		 * Returns no value on empty input, a parse failure, or a percentage with no finite `base`.
		 */
		inline ::std::optional<double> parseLength(::std::string_view value, ::std::optional<double> base) {
			::std::string_view token = trimLength(value);
			if (token.empty()) {
				return ::std::nullopt;
			}

			// Percentage: scale the parent base, or fail when no usable base exists.
			if (token.back() == '%') {
				const ::std::optional<double> percent = parseNumber(trimLength(token.substr(0, token.size() - 1)));
				if (!percent.has_value() || !base.has_value() || !::std::isfinite(*base)) {
					return ::std::nullopt;
				}
				return (*percent / 100.0) * *base;
			}

			// Absolute: an optional `px` unit is stripped, everything else is parsed as a plain number.
			if (token.size() >= 2 && token.substr(token.size() - 2) == "px") {
				token = trimLength(token.substr(0, token.size() - 2));
			}
			return parseNumber(token);
		}
	} // namespace detail

	/**
	 * Resolves a length string into logical pixels.
	 * A bare number (`"12"`) or a `px` value (`"12px"`) is absolute; a percentage (`"5%"`) is taken
	 * relative to `base`, the resolved value of the parent config it inherits from.
	 * On any parse failure (or a percentage with no finite `base`), resolves `fallback` the same way —
	 * typically the config default; if that also fails, returns `0`.
	 */
	inline double resolveLength(const ::std::string& value, const ::std::string& fallback, ::std::optional<double> base = ::std::nullopt) {
		if (const ::std::optional<double> resolved = detail::parseLength(value, base)) {
			return *resolved;
		}
		if (const ::std::optional<double> resolved = detail::parseLength(fallback, base)) {
			return *resolved;
		}
		return 0.0;
	}
} // namespace music_lyric_player::render

#endif // MUSIC_LYRIC_PLAYER_RENDER_UTILS_LENGTH_H_
