.PHONY: format config-generate clean third-party-build windows-build web-build android-build example-windows-build example-web-dev package-windows package-web change-log-build release

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

# Build third-party libraries.
third-party-build:
	cmake -DTHIRD_PARTY_LIBRARY=$(THIRD_PARTY_LIBRARY) -DTHIRD_PARTY_PLATFORM=$(THIRD_PARTY_PLATFORM) -DTHIRD_PARTY_ARCH=$(THIRD_PARTY_ARCH) $(THIRD_PARTY_ARGS) -P cmake/third-party/build.cmake

# Build the Windows module.
windows-build:
	cmake $(BUILD_ARGS) -P cmake/build/windows.cmake

# Build the Web module.
web-build:
	cmake $(BUILD_ARGS) -P cmake/build/web.cmake

# Build the Android module (override the ABI with ANDROID_ABI=x86_64).
android-build:
	cmake -DANDROID_ABI=$(ANDROID_ABI) $(BUILD_ARGS) -P cmake/build/android.cmake

# Build the Windows example.
example-windows-build:
	cmake --preset windows-example
	cmake --build out/example/cache --config Release

# Serve the web example with hot reload.
example-web-dev:
	cd example/web && npm install && npm run dev

# Build and package the Windows native distribution zip.
package-windows:
	cmake -P cmake/package/windows.cmake

# Build and package the Web wasm distribution zip.
package-web:
	cmake -P cmake/package/web.cmake

