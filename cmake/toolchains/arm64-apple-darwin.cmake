set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_SYSTEM_PROCESSOR arm64)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

if(DEFINED ENV{OSXCROSS_ROOT} AND NOT "$ENV{OSXCROSS_ROOT}" STREQUAL "")
  set(LONEJSON_OSXCROSS_ROOT "$ENV{OSXCROSS_ROOT}")
elseif(DEFINED ENV{HOME} AND NOT "$ENV{HOME}" STREQUAL "")
  set(LONEJSON_OSXCROSS_ROOT "$ENV{HOME}/.local/cross/osxcross")
else()
  message(FATAL_ERROR "OSXCROSS_ROOT is not set and HOME is unavailable")
endif()

set(LONEJSON_OSXCROSS_HOST "arm64-apple-darwin25" CACHE STRING
    "osxcross target host triple")
set(LONEJSON_MACOS_DEPLOYMENT_TARGET "15.0" CACHE STRING
    "Minimum macOS deployment target")
set(CMAKE_OSX_DEPLOYMENT_TARGET "${LONEJSON_MACOS_DEPLOYMENT_TARGET}"
    CACHE STRING "" FORCE)

set(LONEJSON_OSXCROSS_BIN_DIR "${LONEJSON_OSXCROSS_ROOT}/bin")
set(ENV{PATH} "${LONEJSON_OSXCROSS_BIN_DIR}:$ENV{PATH}")
set(CMAKE_C_COMPILER
    "${LONEJSON_OSXCROSS_BIN_DIR}/${LONEJSON_OSXCROSS_HOST}-clang"
    CACHE FILEPATH "")
set(CMAKE_AR
    "${LONEJSON_OSXCROSS_BIN_DIR}/${LONEJSON_OSXCROSS_HOST}-ar"
    CACHE FILEPATH "")
set(CMAKE_RANLIB
    "${LONEJSON_OSXCROSS_BIN_DIR}/${LONEJSON_OSXCROSS_HOST}-ranlib"
    CACHE FILEPATH "")
set(CMAKE_LINKER
    "${LONEJSON_OSXCROSS_BIN_DIR}/${LONEJSON_OSXCROSS_HOST}-ld"
    CACHE FILEPATH "")
set(CMAKE_INSTALL_NAME_TOOL
    "${LONEJSON_OSXCROSS_BIN_DIR}/${LONEJSON_OSXCROSS_HOST}-install_name_tool"
    CACHE FILEPATH "")
set(CMAKE_OTOOL
    "${LONEJSON_OSXCROSS_BIN_DIR}/${LONEJSON_OSXCROSS_HOST}-otool"
    CACHE FILEPATH "")
set(CMAKE_STRIP
    "${LONEJSON_OSXCROSS_BIN_DIR}/${LONEJSON_OSXCROSS_HOST}-strip"
    CACHE FILEPATH "")

foreach(_lonejson_required_tool
        CMAKE_C_COMPILER
        CMAKE_AR
        CMAKE_RANLIB
        CMAKE_LINKER
        CMAKE_INSTALL_NAME_TOOL
        CMAKE_OTOOL
        CMAKE_STRIP)
  if(NOT EXISTS "${${_lonejson_required_tool}}")
    message(FATAL_ERROR
      "The arm64 Apple Darwin osxcross toolchain is missing "
      "${_lonejson_required_tool}: ${${_lonejson_required_tool}}. "
      "Set OSXCROSS_ROOT or install osxcross under "
      "$HOME/.local/cross/osxcross.")
  endif()
endforeach()

set(_lonejson_darwin_linker_flag "-fuse-ld=${CMAKE_LINKER}")
string(CONCAT _lonejson_legacy_darwin_linker_regex "(^| )--" "ld-path=[^ ]+")
foreach(_lonejson_linker_flags
        CMAKE_EXE_LINKER_FLAGS
        CMAKE_SHARED_LINKER_FLAGS
        CMAKE_MODULE_LINKER_FLAGS)
  set(_lonejson_current_linker_flags "${${_lonejson_linker_flags}}")
  string(REGEX REPLACE "${_lonejson_legacy_darwin_linker_regex}" ""
         _lonejson_current_linker_flags "${_lonejson_current_linker_flags}")
  string(STRIP "${_lonejson_current_linker_flags}"
         _lonejson_current_linker_flags)
  if(NOT "${_lonejson_current_linker_flags}" MATCHES "(^| )-fuse-ld=")
    set(_lonejson_current_linker_flags
        "${_lonejson_darwin_linker_flag} ${_lonejson_current_linker_flags}")
  endif()
  if(NOT "${${_lonejson_linker_flags}}" STREQUAL
      "${_lonejson_current_linker_flags}")
    set(${_lonejson_linker_flags}
        "${_lonejson_current_linker_flags}"
        CACHE STRING "" FORCE)
  endif()
endforeach()

file(GLOB _lonejson_osxcross_sdks LIST_DIRECTORIES true
     "${LONEJSON_OSXCROSS_ROOT}/SDK/MacOSX*.sdk")
if(NOT _lonejson_osxcross_sdks)
  message(FATAL_ERROR
    "failed to locate a usable osxcross macOS SDK under "
    "${LONEJSON_OSXCROSS_ROOT}/SDK")
endif()
list(SORT _lonejson_osxcross_sdks)
list(REVERSE _lonejson_osxcross_sdks)
list(GET _lonejson_osxcross_sdks 0 LONEJSON_OSXCROSS_SDK)
if(NOT EXISTS "${LONEJSON_OSXCROSS_SDK}/usr/include")
  message(FATAL_ERROR
    "failed to locate a usable osxcross macOS SDK under "
    "${LONEJSON_OSXCROSS_ROOT}/SDK")
endif()

set(CMAKE_OSX_SYSROOT "${LONEJSON_OSXCROSS_SDK}" CACHE PATH "" FORCE)
set(CMAKE_FIND_ROOT_PATH "${LONEJSON_OSXCROSS_SDK}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(LONEJSON_TARGET_ARCH arm64 CACHE STRING "" FORCE)
set(LONEJSON_TARGET_OS darwin CACHE STRING "" FORCE)
set(LONEJSON_TARGET_LIBC "" CACHE STRING "" FORCE)
