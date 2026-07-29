.PHONY: format config-generate clean third-party-build third-party-web-build third-party-lyric-build third-party-skia-build third-party-glaze-build web-build example-windows-build package-windows package-web change-log-build release

# Format Code.
format:
	python script/format-code.py

# Clean Build.
clean:
	cmake -E rm -rf out third-party/lyric/out third-party/skia/out

# Bump version.
release:
	python script/release.py $(RELEASE_ARGS)

# Generate config from JSON schemas.
config-generate:
	python script/generate-config/main.py $(CONFIG_ARGS)

# Build the change log from conventional commits.
change-log-build:
	python script/change-log/build.py $(CHANGE_LOG_ARGS)

# Build All Third-Party Libraries.
third-party-build:
	cmake $(THIRD_PARTY_ARGS) -P cmake/third-party/build.cmake

# Build All Third-Party Libraries For Web (wasm32).
third-party-web-build:
	cmake -DSKIA_BUILD_PLATFORM=web -DLYRIC_BUILD_PLATFORM=web $(THIRD_PARTY_ARGS) -P cmake/third-party/build.cmake

# Build Lyric.
third-party-lyric-build:
	cmake $(LYRIC_ARGS) -P cmake/third-party/lyric/build.cmake

# Build Skia.
third-party-skia-build:
	cmake $(SKIA_ARGS) -P cmake/third-party/skia/build.cmake

# Build Glaze (header-only; verifies the submodule).
third-party-glaze-build:
	cmake $(GLAZE_ARGS) -P cmake/third-party/glaze/build.cmake

# Build the web module (configure, then Release build).
web-build:
	cmake --preset web
	cmake --build --preset web-release

# Build the Windows example (configure, then Release build).
example-windows-build:
	cmake --preset windows-example
	cmake --build --preset windows-example-release

# Build and package the Windows native distribution zip.
package-windows:
	cmake -P cmake/package/windows.cmake

# Build and package the Web wasm distribution zip.
package-web:
	cmake -P cmake/package/web.cmake

