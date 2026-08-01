if(EMSCRIPTEN)
	add_executable(music_lyric_player_web
		web/bindings.cpp
	)

	target_compile_features(music_lyric_player_web PUBLIC cxx_std_20)

	target_compile_definitions(music_lyric_player_web PRIVATE
		MUSIC_LYRIC_PLAYER_VERSION="${MUSIC_LYRIC_PLAYER_VERSION}")

	set_target_properties(music_lyric_player_web PROPERTIES
		OUTPUT_NAME music_lyric_player
		SUFFIX ".mjs"
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out/app/build/${SKIA_PLATFORM}-${SKIA_ARCH}-$<LOWER_CASE:$<CONFIG>>")

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
		# Emits the TypeScript definitions of the embind surface next to the module, which is what the npm package ships as its types.
		# The flag and its file name travel together through SHELL: so option de-duplication cannot split the pair.
		"SHELL:--emit-tsd music_lyric_player.d.ts"
		"$<$<CONFIG:Release>:-O3;-sASSERTIONS=0>")

	# Every dependency stays PRIVATE, exactly as on native, so only the embind surface is reachable from JS.
	target_link_libraries(music_lyric_player_web PRIVATE
		music_lyric_player::backend
		music_lyric_player::rendering
		music_lyric_player::playback
		music_lyric::model
		glaze::glaze)
elseif(ANDROID)
	add_library(music_lyric_player_android SHARED
		android/src/main/cpp/jni.cpp
		android/src/main/cpp/surface.cpp
	)
	add_library(music_lyric_player::android ALIAS music_lyric_player_android)

	target_compile_features(music_lyric_player_android PUBLIC cxx_std_20)

	target_compile_definitions(music_lyric_player_android PRIVATE
		MUSIC_LYRIC_PLAYER_VERSION="${MUSIC_LYRIC_PLAYER_VERSION}")

	set_target_properties(music_lyric_player_android PROPERTIES
		OUTPUT_NAME music_lyric_player
		C_VISIBILITY_PRESET hidden
		CXX_VISIBILITY_PRESET hidden
		VISIBILITY_INLINES_HIDDEN ON
		LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out/app/build/${SKIA_PLATFORM}-${SKIA_ARCH}-$<LOWER_CASE:$<CONFIG>>")

	# The aar ships into arbitrary apps, so nothing beyond the JNI entry points may stay visible.
	# --exclude-libs localises what the static archives bring in, and the version script whitelists the rest, so a host app linking its own Skia cannot collide.
	# A version script cannot reach the __start_ and __stop_ pair the linker synthesises for protobuf's own section, which is what the next option hides instead.
	# Identical code folding stays on the safe setting, which folds only what the address-significance tables mark as never compared by address.
	# The relative relocations of a library this size cost half a megabyte in their plain form, and the packed format bionic has read since Android 6.0 brings that down to tens of kilobytes.
	# The NDK compiles every configuration with -g and leaves the stripping to whoever packages the result, which is us: a release link keeps the debug info out of the artifact altogether, and a debug build is where a symbolised library comes from.
	# The NDK toolchain already passes -ffunction-sections, -fdata-sections and --gc-sections.
	# Bionic only gained __tls_get_addr in API 29, so the toolchain compiles thread-local access as emulated TLS below that, whose helper compiler-rt links in statically.
	# The lyric model is a bitcode archive, though, and the LTO backend does its codegen at link time, where that default is not applied: the option below is what carries it across, and without it the x86_64 link fails on an undefined __tls_get_addr.
	# Arm64 links either way only because lld relaxes the general-dynamic sequence to initial-exec for the non-preemptible symbols a version script leaves hidden, which x86_64 never does for a shared object.
	target_link_options(music_lyric_player_android PRIVATE
		-Wl,-plugin-opt=-emulated-tls
		-Wl,--exclude-libs,ALL
		"-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/android/exports.map"
		-Wl,-z,start-stop-visibility=hidden
		-Wl,--icf=safe
		-Wl,--pack-dyn-relocs=android
		"$<$<CONFIG:Release>:-Wl,-s>")

	target_link_libraries(music_lyric_player_android PRIVATE
		music_lyric_player::backend
		music_lyric_player::rendering
		music_lyric_player::playback
		music_lyric::model
		glaze::glaze)
else()
	add_library(music_lyric_player_native SHARED
		native/abi.cpp
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

	target_link_libraries(music_lyric_player_native PRIVATE
		music_lyric_player::backend
		music_lyric_player::rendering
		music_lyric_player::playback
		music_lyric::model
		glaze::glaze)

	if(WIN32)
		target_link_libraries(music_lyric_player_native PRIVATE music_lyric_player::icu)
	endif()
endif()
