#ifndef MUSIC_LYRIC_PLAYER_UTILS_CONFIG_PROPERTY_H_
#define MUSIC_LYRIC_PLAYER_UTILS_CONFIG_PROPERTY_H_

#include <string>
#include <type_traits>
#include <utility>

namespace music_lyric_player::utils::config {
	/**
	 * A single config leaf: its value plus whether it was explicitly set.
	 * Reads convert implicitly to the value, so most call sites use the leaf as a plain value.
	 * `assigned()` reports whether it was set, which drives sparse merge and inheritance.
	 */
	template <typename T>
	class Property {
	public:
		using value_type = T;

		Property() = default;

		/**
		 * Seeds a default value without marking the leaf set.
		 */
		Property(T value) : stored(std::move(value)) {}

		/**
		 * Seeds a string default from a literal without marking the leaf set.
		 */
		Property(const char* value)
		    requires std::is_same_v<T, std::string>
		    : stored(value) {}

		operator const T&() const {
			return this->stored;
		}

		const T& value() const {
			return this->stored;
		}

		bool assigned() const {
			return this->isSet;
		}

		/**
		 * Overrides the value and marks the leaf set.
		 */
		Property& operator=(T value) {
			this->stored = std::move(value);
			this->isSet  = true;
			return *this;
		}

		/**
		 * Overrides the value from a string literal and marks the leaf set.
		 * A dedicated overload keeps a literal from being an ambiguous match between the value and copy assignments.
		 */
		Property& operator=(const char* value)
		    requires std::is_same_v<T, std::string> {
			this->stored = value;
			this->isSet  = true;
			return *this;
		}

		/**
		 * Compares values only, ignoring the presence flag.
		 */
		friend bool operator==(const Property& a, const Property& b) {
			return a.stored == b.stored;
		}

	private:
		T    stored{};
		bool isSet = false;
	};
} // namespace music_lyric_player::utils::config

#endif // MUSIC_LYRIC_PLAYER_UTILS_CONFIG_PROPERTY_H_
