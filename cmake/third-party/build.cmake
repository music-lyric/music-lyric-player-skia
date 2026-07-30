cmake_minimum_required(VERSION 3.21)

if(NOT CMAKE_SCRIPT_MODE_FILE)
	message(FATAL_ERROR "[ThirdParty] Must be run in script mode")
endif()

if(DEFINED THIRD_PARTY_PLATFORM AND NOT THIRD_PARTY_PLATFORM STREQUAL "")
	set(SKIA_BUILD_PLATFORM "${THIRD_PARTY_PLATFORM}")
	set(LYRIC_BUILD_PLATFORM "${THIRD_PARTY_PLATFORM}")
endif()

if(DEFINED THIRD_PARTY_ARCH AND NOT THIRD_PARTY_ARCH STREQUAL "")
	set(SKIA_BUILD_ARCH "${THIRD_PARTY_ARCH}")
	set(LYRIC_BUILD_ARCH "${THIRD_PARTY_ARCH}")
endif()

set(_known_libraries "lyric" "skia" "glaze")

if(NOT DEFINED THIRD_PARTY_LIBRARY OR THIRD_PARTY_LIBRARY STREQUAL "")
	set(_libraries ${_known_libraries})
else()
	string(TOLOWER "${THIRD_PARTY_LIBRARY}" _library_input)
	if(NOT _library_input IN_LIST _known_libraries)
		list(JOIN _known_libraries ", " _known_libraries_text)
		message(FATAL_ERROR "[ThirdParty] Unsupported library: ${THIRD_PARTY_LIBRARY} (expected one of ${_known_libraries_text})")
	endif()
	set(_libraries "${_library_input}")
endif()

foreach(_library IN LISTS _libraries)
	include("${CMAKE_CURRENT_LIST_DIR}/${_library}/build.cmake")
endforeach()
