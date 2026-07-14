.PHONY: format clean third-party-build third-party-lyric-build third-party-skia-build third-party-glaze-build example-windows-build

# Format Code.
format:
	python script/format-code.py

# Clean.
clean:
	cmake -E rm -rf build third-party/lyric/out third-party/skia/out

# Build All Third-Party Libraries.
third-party-build: third-party-lyric-build third-party-skia-build third-party-glaze-build

# Build Lyric.
third-party-lyric-build:
	cmake $(LYRIC_ARGS) -P cmake/lyric/build.cmake

# Build Skia.
third-party-skia-build:
	cmake $(SKIA_ARGS) -P cmake/skia/build.cmake

# Build Glaze (header-only; verifies the submodule).
third-party-glaze-build:
	cmake $(GLAZE_ARGS) -P cmake/glaze/build.cmake

# Build the Windows example (configure, then Release build).
example-windows-build:
	cmake --preset windows-example
	cmake --build --preset windows-example-release
