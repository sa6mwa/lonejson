cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED LONEJSON_SOURCE_DIR)
  get_filename_component(LONEJSON_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()

set(LONEJSON_LIBLOCKDC_VERSION "0.1.0" CACHE STRING "liblockdc release version to fetch for tests.")
set(LONEJSON_LIBLOCKDC_BASE_URL "https://github.com/sa6mwa/liblockdc/releases/download/v${LONEJSON_LIBLOCKDC_VERSION}" CACHE STRING "Base URL for liblockdc release downloads.")

function(lonejson_detect_host_arch out_var)
  execute_process(
    COMMAND uname -m
    OUTPUT_VARIABLE _uname_m
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _uname_rc)

  if(NOT _uname_rc EQUAL 0)
    message(FATAL_ERROR "Failed to detect host architecture using uname -m.")
  endif()

  if(_uname_m STREQUAL "x86_64" OR _uname_m STREQUAL "amd64")
    set(_arch "x86_64")
  elseif(_uname_m STREQUAL "aarch64" OR _uname_m STREQUAL "arm64")
    set(_arch "aarch64")
  elseif(_uname_m MATCHES "^armv[67].*" OR _uname_m STREQUAL "armhf")
    set(_arch "armhf")
  else()
    message(FATAL_ERROR "Unsupported host architecture '${_uname_m}'. Override with -D LONEJSON_LIBLOCKDC_ARCH=<arch>.")
  endif()

  set(${out_var} "${_arch}" PARENT_SCOPE)
endfunction()

function(lonejson_detect_host_libc out_var)
  execute_process(
    COMMAND ldd --version
    OUTPUT_VARIABLE _ldd_stdout
    ERROR_VARIABLE _ldd_stderr
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _ldd_rc)

  if(NOT _ldd_rc EQUAL 0)
    message(FATAL_ERROR "Failed to detect host libc using ldd --version.")
  endif()

  set(_ldd_text "${_ldd_stdout}\n${_ldd_stderr}")
  string(TOLOWER "${_ldd_text}" _ldd_text_lower)

  if(_ldd_text_lower MATCHES "musl")
    set(_libc "musl")
  elseif(_ldd_text_lower MATCHES "glibc" OR _ldd_text_lower MATCHES "gnu libc")
    set(_libc "gnu")
  else()
    message(FATAL_ERROR "Unable to detect whether host libc is glibc or musl. Override with -D LONEJSON_LIBLOCKDC_LIBC=<gnu|musl>.")
  endif()

  set(${out_var} "${_libc}" PARENT_SCOPE)
endfunction()

if(NOT DEFINED LONEJSON_LIBLOCKDC_ARCH)
  lonejson_detect_host_arch(LONEJSON_LIBLOCKDC_ARCH)
endif()

if(NOT DEFINED LONEJSON_LIBLOCKDC_LIBC)
  lonejson_detect_host_libc(LONEJSON_LIBLOCKDC_LIBC)
endif()

set(_supported_arches x86_64 aarch64 armhf)
set(_supported_libcs gnu musl)

if(NOT LONEJSON_LIBLOCKDC_ARCH IN_LIST _supported_arches)
  message(FATAL_ERROR "Unsupported liblockdc arch '${LONEJSON_LIBLOCKDC_ARCH}'. Supported values: ${_supported_arches}.")
endif()

if(NOT LONEJSON_LIBLOCKDC_LIBC IN_LIST _supported_libcs)
  message(FATAL_ERROR "Unsupported liblockdc libc '${LONEJSON_LIBLOCKDC_LIBC}'. Supported values: ${_supported_libcs}.")
endif()

set(_triple "${LONEJSON_LIBLOCKDC_ARCH}-linux-${LONEJSON_LIBLOCKDC_LIBC}")
set(_filename "liblockdc-${LONEJSON_LIBLOCKDC_VERSION}-${_triple}-dev.tar.gz")
set(_url "${LONEJSON_LIBLOCKDC_BASE_URL}/${_filename}")
set(_deps_root "${LONEJSON_SOURCE_DIR}/.deps/liblockdc/${_triple}")
set(_archive_path "${_deps_root}/${_filename}")
set(_extract_root "${_deps_root}/root")
set(_stamp_path "${_extract_root}/.lonejson-liblockdc-version")

file(MAKE_DIRECTORY "${_deps_root}")

if(EXISTS "${_stamp_path}")
  file(READ "${_stamp_path}" _existing_version)
  string(STRIP "${_existing_version}" _existing_version)
  if(_existing_version STREQUAL "${LONEJSON_LIBLOCKDC_VERSION}")
    message(STATUS "liblockdc ${_triple} dev bundle already extracted at ${_extract_root}")
    return()
  endif()
endif()

message(STATUS "Downloading ${_url}")
file(DOWNLOAD
  "${_url}"
  "${_archive_path}"
  SHOW_PROGRESS
  STATUS _download_status
  TLS_VERIFY ON)
list(GET _download_status 0 _download_code)
list(GET _download_status 1 _download_message)
if(NOT _download_code EQUAL 0)
  message(FATAL_ERROR "Failed to download ${_url}: ${_download_message}")
endif()

file(REMOVE_RECURSE "${_extract_root}")
file(MAKE_DIRECTORY "${_extract_root}")

message(STATUS "Extracting ${_archive_path} to ${_extract_root}")
file(ARCHIVE_EXTRACT INPUT "${_archive_path}" DESTINATION "${_extract_root}")
file(WRITE "${_stamp_path}" "${LONEJSON_LIBLOCKDC_VERSION}\n")

message(STATUS "liblockdc ${_triple} dev bundle ready at ${_extract_root}")
