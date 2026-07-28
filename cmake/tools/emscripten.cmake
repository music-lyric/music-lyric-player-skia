include("${CMAKE_CURRENT_LIST_DIR}/../common/emsdk.cmake")

# Chain to the toolchain file the resolved emsdk ships.
# Presets cannot run CMake code, so routing CMAKE_TOOLCHAIN_FILE through here is what lets the web preset fall back to the emsdk Skia pins instead of demanding an EMSDK environment variable.
emsdk_resolve("" _emsdk_dir)
emsdk_require_compiler("${_emsdk_dir}")
emsdk_resolve_toolchain("${_emsdk_dir}" _emsdk_toolchain)

include("${_emsdk_toolchain}")
