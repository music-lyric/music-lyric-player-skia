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
	rendering/components/container/index.cpp
	rendering/components/line/normal/index.cpp
	rendering/components/line/normal/annotation/index.cpp
	rendering/components/line/normal/main/plain/index.cpp
	rendering/components/line/normal/main/syllable/index.cpp
	rendering/components/line/normal/main/syllable/word.cpp
	rendering/components/line/normal/main/syllable/animation/emphasize.cpp
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

# The backend is the only layer that differs per platform.
# Windows draws through Vulkan and resolves families from DirectWrite, while the web draws through WebGL
# and has no system font stack to resolve from at all.
if(EMSCRIPTEN)
	set(_backend_sources
		backend/gpu/webgl.cpp
		backend/font/font_web.cpp
	)
else()
	find_package(Vulkan REQUIRED)

	set(_backend_sources
		backend/gpu/vulkan.cpp
		backend/font/font_win.cpp
	)
endif()

add_library(music_lyric_player_backend ${_backend_sources})
add_library(music_lyric_player::backend ALIAS music_lyric_player_backend)

target_compile_features(music_lyric_player_backend PUBLIC cxx_std_20)

target_include_directories(music_lyric_player_backend PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(music_lyric_player_backend PUBLIC
	skia::skia)

if(NOT EMSCRIPTEN)
	target_link_libraries(music_lyric_player_backend PUBLIC
		Vulkan::Vulkan)
endif()

# Embed ICU data on Windows, where Skia leaves SkLoadICU() for the embedder to supply.
# Other platforms assemble ICU in through Skia itself, so this path is Windows-only.
# SKIA_ICU_PROFILE selects the size: flutter (~0.74MB, default) or common (~10MB, full CJK dictionary).
if(WIN32)
	if(NOT DEFINED SKIA_ICU_PROFILE)
		set(SKIA_ICU_PROFILE "flutter" CACHE STRING "ICU data profile to embed: flutter (minimal) or common (full)")
	endif()

	set(_icu_data_dat "${CMAKE_SOURCE_DIR}/out/third-party/skia/icu/${SKIA_ICU_PROFILE}/icudtl.dat")
	if(NOT EXISTS "${_icu_data_dat}")
		message(FATAL_ERROR "[ICU] Profile data not found: ${_icu_data_dat} (build Skia once to stage the ICU profiles into out/, or set SKIA_ICU_PROFILE)")
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

if(EMSCRIPTEN)
	# Emscripten builds a loadable module out of an executable rather than a library.
	add_executable(music_lyric_player_web
		platform/web/bindings.cpp
	)

	target_compile_features(music_lyric_player_web PUBLIC cxx_std_20)

	target_compile_definitions(music_lyric_player_web PRIVATE
		MUSIC_LYRIC_PLAYER_VERSION="${MUSIC_LYRIC_PLAYER_VERSION}")

	set_target_properties(music_lyric_player_web PROPERTIES
		OUTPUT_NAME music_lyric_player
		SUFFIX ".mjs"
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out/app/build/${SKIA_PLATFORM}-${SKIA_ARCH}-$<LOWER_CASE:$<CONFIG>>")

	# Skia is built as CanvasKit, whose own wasm link config is empty, so every flag the module needs comes from here.
	# There is no main() because JS drives the module through embind, and the runtime stays resident between calls.
	target_link_options(music_lyric_player_web PRIVATE
		-lembind
		--no-entry
		-fwasm-exceptions
		-sMODULARIZE=1
		-sEXPORT_ES6=1
		-sEXPORT_NAME=createMusicLyricPlayerModule
		-sENVIRONMENT=web
		-sALLOW_MEMORY_GROWTH=1
		-sINITIAL_MEMORY=64MB
		-sMIN_WEBGL_VERSION=2
		-sMAX_WEBGL_VERSION=2
		-sEXIT_RUNTIME=0
		-sSTACK_SIZE=1048576
		"$<$<CONFIG:Release>:-O3;-sASSERTIONS=0>")

	# Every dependency stays PRIVATE, exactly as on native, so only the embind surface is reachable from JS.
	target_link_libraries(music_lyric_player_web PRIVATE
		music_lyric_player::backend
		music_lyric_player::rendering
		music_lyric_player::playback
		music_lyric::model
		glaze::glaze)
else()
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
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out/app/build/${SKIA_PLATFORM}-${SKIA_ARCH}-$<LOWER_CASE:$<CONFIG>>"
		LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out/app/build/${SKIA_PLATFORM}-${SKIA_ARCH}-$<LOWER_CASE:$<CONFIG>>"
		ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out/app/build/${SKIA_PLATFORM}-${SKIA_ARCH}-$<LOWER_CASE:$<CONFIG>>")

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
endif()
