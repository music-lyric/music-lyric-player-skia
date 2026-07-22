.PHONY: format clean third-party-build third-party-lyric-build third-party-skia-build third-party-glaze-build example-windows-build package-windows change-log-build release

# Format Code.
format:
	python script/format-code.py

# Clean.
clean:
	cmake -E rm -rf out third-party/lyric/out third-party/skia/out

# Build All Third-Party Libraries.
third-party-build:
	cmake $(THIRD_PARTY_ARGS) -P cmake/third-party/build.cmake

# Build Lyric.
third-party-lyric-build:
	cmake $(LYRIC_ARGS) -P cmake/third-party/lyric/build.cmake

# Build Skia.
third-party-skia-build:
	cmake $(SKIA_ARGS) -P cmake/third-party/skia/build.cmake

# Build Glaze (header-only; verifies the submodule).
third-party-glaze-build:
	cmake $(GLAZE_ARGS) -P cmake/third-party/glaze/build.cmake

# Build the Windows example (configure, then Release build).
example-windows-build:
	cmake --preset windows-example
	cmake --build --preset windows-example-release

# Build and package the Windows native distribution zip.
package-windows:
	cmake -P cmake/package/windows.cmake

# Build the change log from conventional commits.
change-log-build:
	python script/change-log/build.py $(CHANGE_LOG_ARGS)

# Bump version.
release:
	python script/release.py $(RELEASE_ARGS)
