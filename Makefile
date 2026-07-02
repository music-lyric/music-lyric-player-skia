.PHONY: format skia-sync skia-build

# Format Code.
format:
	python script/format-code.py

# Sync Skia Deps.
skia-sync:
	cd third-party/skia && python tools/git-sync-deps

# Build Skia.
skia-build:
	cmake $(SKIA_ARGS) -P cmake/skia/build.cmake
