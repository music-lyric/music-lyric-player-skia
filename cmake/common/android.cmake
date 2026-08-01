include_guard(GLOBAL)

# The NDK revision every Android artifact is built with.
# It is pinned rather than probed because the prebuilt Skia and lyric archives carry LLVM bitcode, which only links against the LLVM that same NDK ships.
function(android_ndk_pinned_version out_version)
	set(${out_version} "29.0.14206865" PARENT_SCOPE)
endfunction()

# Resolve the NDK install directory: an explicit hint first, then the two NDK environment variables, then the pinned revision under the SDK.
function(android_resolve_ndk hint out_dir)
	android_ndk_pinned_version(_pinned)

	if(NOT "${hint}" STREQUAL "")
		set(_dir "${hint}")
	elseif(DEFINED ENV{ANDROID_NDK_ROOT} AND NOT "$ENV{ANDROID_NDK_ROOT}" STREQUAL "")
		set(_dir "$ENV{ANDROID_NDK_ROOT}")
	elseif(DEFINED ENV{ANDROID_NDK_HOME} AND NOT "$ENV{ANDROID_NDK_HOME}" STREQUAL "")
		set(_dir "$ENV{ANDROID_NDK_HOME}")
	elseif(DEFINED ENV{ANDROID_HOME} AND NOT "$ENV{ANDROID_HOME}" STREQUAL "")
		set(_dir "$ENV{ANDROID_HOME}/ndk/${_pinned}")
	elseif(DEFINED ENV{ANDROID_SDK_ROOT} AND NOT "$ENV{ANDROID_SDK_ROOT}" STREQUAL "")
		set(_dir "$ENV{ANDROID_SDK_ROOT}/ndk/${_pinned}")
	else()
		set(_dir "")
	endif()

	file(TO_CMAKE_PATH "${_dir}" _dir)
	set(${out_dir} "${_dir}" PARENT_SCOPE)
endfunction()

# Read the package revision an NDK records.
function(android_ndk_version dir out_version)
	set(_version "")
	set(_props "${dir}/source.properties")

	if(EXISTS "${_props}")
		file(STRINGS "${_props}" _lines REGEX "^Pkg\\.Revision[ \t]*=")
		if(_lines)
			list(GET _lines 0 _line)
			string(REGEX REPLACE "^Pkg\\.Revision[ \t]*=[ \t]*" "" _version "${_line}")
			string(STRIP "${_version}" _version)
		endif()
	endif()

	set(${out_version} "${_version}" PARENT_SCOPE)
endfunction()

# Fail unless `dir` holds the pinned NDK.
function(android_require_ndk dir)
	if("${dir}" STREQUAL "" OR NOT EXISTS "${dir}/build/cmake/android.toolchain.cmake")
		message(FATAL_ERROR "[Android] NDK not found: ${dir}")
	endif()

	# A partially unpacked NDK still carries the toolchain file, so check the compiler instead.
	file(GLOB _compilers
		"${dir}/toolchains/llvm/prebuilt/*/bin/clang++"
		"${dir}/toolchains/llvm/prebuilt/*/bin/clang++.exe")
	if(NOT _compilers)
		message(FATAL_ERROR "[Android] NDK is missing its LLVM prebuilt: ${dir}/toolchains/llvm/prebuilt")
	endif()

	# An override that points somewhere else has to fail loudly, because a version split only surfaces as a link error much later.
	android_ndk_pinned_version(_pinned)
	android_ndk_version("${dir}" _version)
	if(NOT "${_version}" STREQUAL "${_pinned}")
		message(FATAL_ERROR "[Android] NDK ${_pinned} required, found ${_version}: ${dir}")
	endif()
endfunction()

# Resolve the CMake toolchain file that cross-compiles to Android through `dir`.
function(android_resolve_toolchain dir out_file)
	set(_file "${dir}/build/cmake/android.toolchain.cmake")
	if(NOT EXISTS "${_file}")
		message(FATAL_ERROR "[Android] Toolchain file not found: ${_file}")
	endif()

	set(${out_file} "${_file}" PARENT_SCOPE)
endfunction()

# Resolve one of the LLVM binutils the NDK ships, which sit under a host-specific prebuilt directory and never on PATH.
function(android_resolve_llvm_tool dir name out_file)
	file(GLOB _candidates
		"${dir}/toolchains/llvm/prebuilt/*/bin/${name}"
		"${dir}/toolchains/llvm/prebuilt/*/bin/${name}.exe")
	if(NOT _candidates)
		message(FATAL_ERROR "[Android] NDK is missing ${name}: ${dir}/toolchains/llvm/prebuilt")
	endif()

	list(GET _candidates 0 _file)
	set(${out_file} "${_file}" PARENT_SCOPE)
endfunction()
