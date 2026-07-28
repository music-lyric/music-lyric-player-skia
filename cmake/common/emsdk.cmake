include_guard(GLOBAL)

# Resolve the emsdk install directory: an explicit hint first, then the EMSDK environment variable, then the copy Skia pins under its externals.
function(emsdk_resolve hint out_dir)
	if(NOT "${hint}" STREQUAL "")
		set(_dir "${hint}")
	elseif(DEFINED ENV{EMSDK} AND NOT "$ENV{EMSDK}" STREQUAL "")
		set(_dir "$ENV{EMSDK}")
	else()
		get_filename_component(_repo_root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../.." ABSOLUTE)
		set(_dir "${_repo_root}/third-party/skia/third_party/externals/emsdk")
	endif()

	file(TO_CMAKE_PATH "${_dir}" _dir)
	set(${out_dir} "${_dir}" PARENT_SCOPE)
endfunction()

function(emsdk_require_compiler dir)
	if(CMAKE_HOST_WIN32)
		set(_compiler "${dir}/upstream/emscripten/em++.bat")
	else()
		set(_compiler "${dir}/upstream/emscripten/em++")
	endif()

	if(NOT EXISTS "${_compiler}")
		message(FATAL_ERROR
			"[Emsdk] Compiler not found: ${_compiler}\n"
			"        Bootstrap the pinned emsdk once:\n"
			"          python third-party/skia/tools/git-sync-deps\n"
			"          python third-party/skia/bin/activate-emsdk\n"
			"        Or set EMSDK to an install matching the version in third-party/skia/bin/activate-emsdk")
	endif()
endfunction()

# Resolve the CMake toolchain file that cross-compiles to wasm through `dir`.
function(emsdk_resolve_toolchain dir out_file)
	set(_file "${dir}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake")
	if(NOT EXISTS "${_file}")
		message(FATAL_ERROR "[Emsdk] Toolchain file not found: ${_file}")
	endif()

	set(${out_file} "${_file}" PARENT_SCOPE)
endfunction()
