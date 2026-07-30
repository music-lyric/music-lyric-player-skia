include("${CMAKE_CURRENT_LIST_DIR}/../common/android.cmake")

# Chain to the toolchain file the resolved NDK ships.
# Presets cannot run CMake code, so routing CMAKE_TOOLCHAIN_FILE through here is what lets the android preset find a side-by-side NDK instead of demanding an ANDROID_NDK_ROOT environment variable.
# No include guard here: CMake reads a toolchain file twice, and a guard would turn the second read into a no-op.
android_resolve_ndk("" _ndk_dir)
android_require_ndk("${_ndk_dir}")
android_resolve_toolchain("${_ndk_dir}" _ndk_toolchain)

include("${_ndk_toolchain}")
