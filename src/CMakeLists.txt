find_package(Vulkan REQUIRED)

add_library(music_lyric_player_playback
	utils/clock/steady.cpp
	playback/merger.cpp
	playback/offset.cpp
	playback/player.cpp
)
add_library(music_lyric_player::playback ALIAS music_lyric_player_playback)

target_compile_features(music_lyric_player_playback PUBLIC cxx_std_20)

target_include_directories(music_lyric_player_playback PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(music_lyric_player_playback PUBLIC
	music_lyric::model)

add_library(music_lyric_player_rendering
	rendering/components/line/normal/index.cpp
	rendering/components/line/normal/main/plain/index.cpp
	rendering/components/line/normal/main/syllable/index.cpp
	rendering/components/line/normal/main/syllable/word.cpp
	rendering/components/line/normal/main/syllable/animation/float.cpp
	rendering/components/line/normal/main/syllable/animation/mask.cpp
	rendering/components/line/interlude/index.cpp
	rendering/core/effect.cpp
	rendering/core/layout.cpp
	rendering/core/line.cpp
	rendering/core/scroll.cpp
	rendering/core/transition.cpp
	rendering/renderer.cpp
)
add_library(music_lyric_player::rendering ALIAS music_lyric_player_rendering)

target_compile_features(music_lyric_player_rendering PUBLIC cxx_std_20)

target_include_directories(music_lyric_player_rendering PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(music_lyric_player_rendering PUBLIC
	music_lyric_player::playback
	music_lyric::model
	skia::skia)

add_library(music_lyric_player_backend
	backend/gpu/vulkan.cpp
	backend/font/font_win.cpp
)
add_library(music_lyric_player::backend ALIAS music_lyric_player_backend)

target_compile_features(music_lyric_player_backend PUBLIC cxx_std_20)

target_include_directories(music_lyric_player_backend PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(music_lyric_player_backend PUBLIC
	skia::skia
	Vulkan::Vulkan)

# Embed ICU data on Windows, where Skia leaves SkLoadICU() for the embedder to supply.
# Other platforms assemble ICU in through Skia itself, so this path is Windows-only.
# SKIA_ICU_PROFILE selects the size: flutter (~0.74MB, default) or common (~10MB, full CJK dictionary).
if(WIN32)
	if(NOT DEFINED SKIA_ICU_PROFILE)
		set(SKIA_ICU_PROFILE "flutter" CACHE STRING "ICU data profile to embed: flutter (minimal) or common (full)")
	endif()

	set(_icu_data_dat "${SKIA_SRC}/third_party/externals/icu/${SKIA_ICU_PROFILE}/icudtl.dat")
	if(NOT EXISTS "${_icu_data_dat}")
		message(FATAL_ERROR "[ICU] Profile data not found: ${_icu_data_dat} (build Skia once to sync externals, or set SKIA_ICU_PROFILE)")
	endif()

	find_program(MUSIC_LYRIC_PLAYER_PYTHON NAMES python python3 REQUIRED)
	set(_icu_data_gen "${CMAKE_CURRENT_BINARY_DIR}/gen/icu_data.gen.cpp")
	set(_icu_data_script "${CMAKE_SOURCE_DIR}/script/generate-icu-data.py")

	add_custom_command(
		OUTPUT "${_icu_data_gen}"
		COMMAND "${MUSIC_LYRIC_PLAYER_PYTHON}" "${_icu_data_script}" "${_icu_data_dat}" "${_icu_data_gen}"
		DEPENDS "${_icu_data_script}" "${_icu_data_dat}"
		COMMENT "Embedding ICU data (profile: ${SKIA_ICU_PROFILE})"
		VERBATIM)

	# An object library so the override and blob link directly into each artifact, letting our SkLoadICU shadow Skia's without a duplicate symbol.
	add_library(music_lyric_player_icu OBJECT
		backend/icu/load.cpp
		"${_icu_data_gen}"
	)
	add_library(music_lyric_player::icu ALIAS music_lyric_player_icu)

	target_compile_features(music_lyric_player_icu PUBLIC cxx_std_20)
endif()

add_library(music_lyric_player_native SHARED
	platform/native/abi.cpp
)
add_library(music_lyric_player::native ALIAS music_lyric_player_native)

target_compile_features(music_lyric_player_native PUBLIC cxx_std_20)

# Building the shared library exports the public C ABI as dllexport through MUSIC_LYRIC_PLAYER_API.
# Every other symbol, from Skia and ICU to protobuf and the C++ runtime, stays hidden inside the DLL.
target_compile_definitions(music_lyric_player_native PRIVATE MUSIC_LYRIC_PLAYER_BUILDING)

# Non-MSVC toolchains need these presets to hide unmarked symbols, which MSVC already does by default.
# They pair with the visibility("default") branch in export.h so the narrow ABI holds on every platform.
set_target_properties(music_lyric_player_native PROPERTIES
	C_VISIBILITY_PRESET hidden
	CXX_VISIBILITY_PRESET hidden
	VISIBILITY_INLINES_HIDDEN ON
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out/app/${SKIA_PLATFORM}-${SKIA_ARCH}-$<LOWER_CASE:$<CONFIG>>"
	LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out/app/${SKIA_PLATFORM}-${SKIA_ARCH}-$<LOWER_CASE:$<CONFIG>>"
	ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out/app/${SKIA_PLATFORM}-${SKIA_ARCH}-$<LOWER_CASE:$<CONFIG>>")

target_include_directories(music_lyric_player_native PUBLIC "${CMAKE_SOURCE_DIR}/include")

# The engine and Glaze are absorbed statically into the DLL and never surface in the C ABI headers.
# Every dependency stays PRIVATE, so consumers link the DLL alone and see only the include/ headers.
# Glaze additionally raises this target to its required C++23 without imposing that on consumers.
target_link_libraries(music_lyric_player_native PRIVATE
	music_lyric_player::backend
	music_lyric_player::rendering
	music_lyric_player::playback
	music_lyric::model
	glaze::glaze)

# The embedded ICU data and SkLoadICU override must sit in the final DLL link, next to Skia's text stack.
if(WIN32)
	target_link_libraries(music_lyric_player_native PRIVATE music_lyric_player::icu)
endif()
