#ifndef MUSIC_LYRIC_PLAYER_UTILS_CONFIG_PROPERTY_GLAZE_H_
#define MUSIC_LYRIC_PLAYER_UTILS_CONFIG_PROPERTY_GLAZE_H_

#include "glaze/json.hpp"

#include "utils/config/property.h"

template <typename T>
struct glz::from<glz::JSON, ::music_lyric_player::utils::config::Property<T>> {
	/**
	 * Parses the present JSON value into the property, which marks it set.
	 */
	template <auto Opts, class... Args>
	static void op(auto&& property, glz::is_context auto&& ctx, Args&&... args) {
		T parsed{};
		glz::parse<glz::JSON>::op<Opts>(parsed, ctx, args...);
		property = std::move(parsed);
	}
};

template <typename T>
struct glz::to<glz::JSON, ::music_lyric_player::utils::config::Property<T>> {
	/**
	 * Writes the property's underlying value.
	 */
	template <auto Opts, class... Args>
	static void op(auto&& property, glz::is_context auto&& ctx, Args&&... args) {
		glz::serialize<glz::JSON>::op<Opts>(property.value(), ctx, args...);
	}
};

#endif // MUSIC_LYRIC_PLAYER_UTILS_CONFIG_PROPERTY_GLAZE_H_
