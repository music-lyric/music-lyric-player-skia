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
	rendering/components/container/container.cpp
	rendering/components/line/normal/normal.cpp
	rendering/components/line/normal/annotation/annotation.cpp
	rendering/components/line/normal/main/plain/plain.cpp
	rendering/components/line/normal/main/syllable/syllable.cpp
	rendering/components/line/normal/main/syllable/word.cpp
	rendering/components/line/normal/main/syllable/animation/emphasize.cpp
	rendering/components/line/normal/main/syllable/animation/float.cpp
	rendering/components/line/normal/main/syllable/animation/mask.cpp
	rendering/components/line/interlude/interlude.cpp
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
# Windows draws through Vulkan and resolves families from DirectWrite, Android draws through EGL, and the web draws through WebGL with no system font stack at all.
if(EMSCRIPTEN)
	set(_backend_sources
		backend/gpu/webgl/webgl.cpp
		backend/font/font_web.cpp
		backend/font/registry.cpp
	)
	set(_backend_libraries "")
elseif(ANDROID)
	set(_backend_sources
		backend/gpu/vulkan/vulkan.cpp
		backend/gpu/vulkan/platform_android.cpp
		backend/gpu/gles/gles.cpp
		backend/font/registry.cpp
	)
	# libvulkan reached the platform in Android 7.0, so this is what pins the module to android-24.
	# EGL stands the GLES context up, and GLESv2 resolves the entry points Skia calls straight rather than through the interface it loads at runtime.
	# libandroid carries ANativeWindow, and liblog is where the logger has to write because an Android process has no usable stderr.
	set(_backend_libraries vulkan EGL GLESv2 android log)
else()
	find_package(Vulkan REQUIRED)

	set(_backend_sources
		backend/gpu/vulkan/vulkan.cpp
		backend/gpu/vulkan/platform_windows.cpp
		backend/font/font_win.cpp
		backend/font/registry.cpp
	)
	set(_backend_libraries Vulkan::Vulkan)
endif()

add_library(music_lyric_player_backend ${_backend_sources})
add_library(music_lyric_player::backend ALIAS music_lyric_player_backend)

target_compile_features(music_lyric_player_backend PUBLIC cxx_std_20)

target_include_directories(music_lyric_player_backend PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(music_lyric_player_backend PUBLIC
	skia::skia
	${_backend_libraries})

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
