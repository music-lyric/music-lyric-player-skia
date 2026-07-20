/*
 * Project-side override of Skia's SkLoadICU().
 *
 * Skia leaves SkLoadICU() unresolved on Windows so the embedder can supply ICU's data.
 * We register the embedded music_lyric_player_icu_data blob, so no external icudtl.dat ships and Skia stays unmodified.
 * Linking this object directly into each artifact lets its SkLoadICU shadow Skia's without a duplicate symbol.
 */

#include <cstdint>

// Declared by hand because Skia builds ICU with U_DISABLE_RENAMING=1, so the symbol keeps its plain name.
extern "C" void udata_setCommonData(const void* data, int* status);

// Defined by the generated translation unit (see script/generate-icu-data.py).
extern const std::uint32_t music_lyric_player_icu_data[];

/**
 * Registers the embedded ICU data with the runtime once and reports whether it succeeded.
 * Skia calls this from SkUnicodes::ICU::Make(), where a false return disables all text shaping.
 */
bool SkLoadICU() {
	static const bool loaded = [] {
		int status = 0;
		udata_setCommonData(music_lyric_player_icu_data, &status);
		return status <= 0;
	}();
	return loaded;
}
