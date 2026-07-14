cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED LONEJSON_SOURCE_DIR)
  get_filename_component(LONEJSON_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()

include("${CMAKE_CURRENT_LIST_DIR}/c_pkt_systems_metadata.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/CpktDependencyCache.cmake")

if(DEFINED LONEJSON_C_PKT_SYSTEMS_VERSION AND
   NOT LONEJSON_C_PKT_SYSTEMS_VERSION STREQUAL LONEJSON_C_PKT_SYSTEMS_PINNED_VERSION)
  message(FATAL_ERROR
    "LONEJSON_C_PKT_SYSTEMS_VERSION is not configurable. This repository "
    "pins c.pkt.systems ${LONEJSON_C_PKT_SYSTEMS_PINNED_VERSION} and its "
    "checksums; update cmake/fetch_c_pkt_systems.cmake to change the bundle "
    "version.")
endif()
set(LONEJSON_C_PKT_SYSTEMS_VERSION "${LONEJSON_C_PKT_SYSTEMS_PINNED_VERSION}")
set(LONEJSON_C_PKT_SYSTEMS_BASE_URL
    "${LONEJSON_C_PKT_SYSTEMS_BASE_URL_DEFAULT}"
    CACHE STRING "Base URL for c.pkt.systems release downloads.")
set(LONEJSON_C_PKT_SYSTEMS_DOWNLOAD_RETRIES "4" CACHE STRING
    "Number of download attempts for c.pkt.systems bundles before failing.")
set(LONEJSON_C_PKT_SYSTEMS_RETRY_DELAY_SECONDS "1" CACHE STRING
    "Seconds to wait between c.pkt.systems bundle download retries.")

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
    message(FATAL_ERROR
      "Unsupported host architecture '${_uname_m}'. Override with "
      "-D LONEJSON_C_PKT_SYSTEMS_TARGET_ID=<target>.")
  endif()

  set(${out_var} "${_arch}" PARENT_SCOPE)
endfunction()

function(lonejson_detect_host_os out_var)
  execute_process(
    COMMAND uname -s
    OUTPUT_VARIABLE _uname_s
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _uname_rc)

  if(NOT _uname_rc EQUAL 0)
    message(FATAL_ERROR "Failed to detect host OS using uname -s.")
  endif()
  string(TOLOWER "${_uname_s}" _uname_s_lower)
  set(${out_var} "${_uname_s_lower}" PARENT_SCOPE)
endfunction()

function(lonejson_detect_host_libc out_var)
  execute_process(
    COMMAND ldd --version
    OUTPUT_VARIABLE _ldd_stdout
    ERROR_VARIABLE _ldd_stderr
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _ldd_rc)

  set(_ldd_text "${_ldd_stdout}\n${_ldd_stderr}")
  string(TOLOWER "${_ldd_text}" _ldd_text_lower)

  if(_ldd_text_lower MATCHES "musl")
    set(_libc "musl")
  elseif(_ldd_text_lower MATCHES "glibc" OR _ldd_text_lower MATCHES "gnu libc")
    set(_libc "gnu")
  else()
    message(FATAL_ERROR
      "Unable to detect whether host libc is glibc or musl from ldd --version. Override with "
      "-D LONEJSON_C_PKT_SYSTEMS_TARGET_ID=<target>.")
  endif()

  set(${out_var} "${_libc}" PARENT_SCOPE)
endfunction()

if(NOT DEFINED LONEJSON_C_PKT_SYSTEMS_TARGET_ID)
  lonejson_detect_host_os(_host_os)
  lonejson_detect_host_arch(_host_arch)
  if(_host_os STREQUAL "darwin" AND _host_arch STREQUAL "aarch64")
    set(LONEJSON_C_PKT_SYSTEMS_TARGET_ID "arm64-apple-darwin")
  elseif(_host_os STREQUAL "linux")
    lonejson_detect_host_libc(_host_libc)
    set(LONEJSON_C_PKT_SYSTEMS_TARGET_ID "${_host_arch}-linux-${_host_libc}")
  else()
    message(FATAL_ERROR
      "Unsupported host target '${_host_arch}-${_host_os}'. Override with "
      "-D LONEJSON_C_PKT_SYSTEMS_TARGET_ID=<target>.")
  endif()
endif()

set(_supported_targets
  x86_64-linux-gnu
  x86_64-linux-musl
  aarch64-linux-gnu
  aarch64-linux-musl
  armhf-linux-gnu
  armhf-linux-musl
  arm64-apple-darwin)

if(NOT LONEJSON_C_PKT_SYSTEMS_TARGET_ID IN_LIST _supported_targets)
  message(FATAL_ERROR
    "Unsupported c.pkt.systems target '${LONEJSON_C_PKT_SYSTEMS_TARGET_ID}'. "
    "Supported values: ${_supported_targets}.")
endif()

function(lonejson_download_c_pkt_systems_bundle url archive_name expected_sha256 out_var)
  if(NOT LONEJSON_C_PKT_SYSTEMS_DOWNLOAD_RETRIES MATCHES "^[0-9]+$")
    message(FATAL_ERROR
      "LONEJSON_C_PKT_SYSTEMS_DOWNLOAD_RETRIES must be a non-negative integer.")
  endif()
  if(NOT LONEJSON_C_PKT_SYSTEMS_RETRY_DELAY_SECONDS MATCHES "^[0-9]+$")
    message(FATAL_ERROR
      "LONEJSON_C_PKT_SYSTEMS_RETRY_DELAY_SECONDS must be a non-negative integer.")
  endif()
  if(LONEJSON_C_PKT_SYSTEMS_DOWNLOAD_RETRIES LESS 1)
    message(FATAL_ERROR
      "LONEJSON_C_PKT_SYSTEMS_DOWNLOAD_RETRIES must be at least 1.")
  endif()

  if(DEFINED LONEJSON_C_PKT_SYSTEMS_TEST_FAIL_DOWNLOAD_ATTEMPTS)
    set(CPKT_DEPENDENCY_CACHE_TEST_FAIL_DOWNLOAD_ATTEMPTS
      "${LONEJSON_C_PKT_SYSTEMS_TEST_FAIL_DOWNLOAD_ATTEMPTS}")
  endif()
  cpkt_acquire_verified_archive(
    COMPONENT "c.pkt.systems"
    URL "${url}"
    SHA256 "${expected_sha256}"
    ARCHIVE_NAME "${archive_name}"
    OUTPUT_VARIABLE _archive_path
    RETRIES "${LONEJSON_C_PKT_SYSTEMS_DOWNLOAD_RETRIES}"
    RETRY_DELAY_SECONDS "${LONEJSON_C_PKT_SYSTEMS_RETRY_DELAY_SECONDS}")
  set(${out_var} "${_archive_path}" PARENT_SCOPE)
endfunction()

set(_target_id "${LONEJSON_C_PKT_SYSTEMS_TARGET_ID}")
set(_bundle_dir_name "c.pkt.systems-${LONEJSON_C_PKT_SYSTEMS_VERSION}-${_target_id}")
set(_filename "${_bundle_dir_name}.tar.gz")
set(_url "${LONEJSON_C_PKT_SYSTEMS_BASE_URL}/${_filename}")
set(_deps_root "${LONEJSON_SOURCE_DIR}/.cache/c.pkt.systems/${_target_id}")
set(_extract_root "${_deps_root}/root")
set(_staging_root "${_deps_root}/extract")
set(_stamp_path "${_extract_root}/.lonejson-c-pkt-systems-identity")
set(_extract_lock_path "${_deps_root}/.lonejson-c-pkt-systems-extract.lock")
lonejson_c_pkt_systems_sha256("${_target_id}" _expected_sha256)
if(DEFINED LONEJSON_C_PKT_SYSTEMS_EXPECTED_SHA256_OVERRIDE)
  set(_expected_sha256 "${LONEJSON_C_PKT_SYSTEMS_EXPECTED_SHA256_OVERRIDE}")
endif()
set(_dependency_identity
  "cache-layout=2\ncomponent=c.pkt.systems\nversion=${LONEJSON_C_PKT_SYSTEMS_VERSION}\ntarget=${_target_id}\nurl=${_url}\nsha256=${_expected_sha256}\n")

function(lonejson_validate_c_pkt_systems_bundle extract_root filename target_id curl_version_out openssl_version_out)
  set(_curl_version_header "${extract_root}/include/curl/curlver.h")
  set(_openssl_version_header "${extract_root}/include/openssl/opensslv.h")
  if(target_id MATCHES "apple-darwin$")
    set(_shared_ext "dylib")
  else()
    set(_shared_ext "so")
  endif()

  foreach(_required_path
      "${_curl_version_header}"
      "${_openssl_version_header}"
      "${extract_root}/include/nghttp2/nghttp2.h"
      "${extract_root}/include/zlib.h"
      "${extract_root}/lib/libcurl.${_shared_ext}"
      "${extract_root}/lib/libssl.${_shared_ext}"
      "${extract_root}/lib/libcrypto.${_shared_ext}"
      "${extract_root}/lib/cmake/CURL/CURLConfig.cmake"
      "${extract_root}/lib/cmake/OpenSSL/OpenSSLConfig.cmake"
      "${extract_root}/lib/cmake/nghttp2/nghttp2Config.cmake"
      "${extract_root}/lib/cmake/zlib/ZLIBConfig.cmake"
      "${extract_root}/lib/cmake/libssh2/libssh2-config.cmake"
      "${extract_root}/lib/pkgconfig/libcurl.pc"
      "${extract_root}/lib/pkgconfig/libcrypto.pc")
    if(NOT EXISTS "${_required_path}")
      message(FATAL_ERROR
        "Downloaded c.pkt.systems bundle ${filename} is missing required "
        "file: ${_required_path}")
    endif()
  endforeach()

  file(STRINGS "${_curl_version_header}" _curl_version_line
       REGEX "^#define LIBCURL_VERSION \"[^\"]+\"")
  file(STRINGS "${_openssl_version_header}" _openssl_version_line
       REGEX "^# define OPENSSL_VERSION_TEXT \"[^\"]+\"")
  string(REGEX REPLACE "^#define LIBCURL_VERSION \"([^\"]+)\".*$" "\\1"
         _curl_version "${_curl_version_line}")
  string(REGEX REPLACE "^# define OPENSSL_VERSION_TEXT \"([^\"]+)\".*$" "\\1"
         _openssl_version "${_openssl_version_line}")

  set(${curl_version_out} "${_curl_version}" PARENT_SCOPE)
  set(${openssl_version_out} "${_openssl_version}" PARENT_SCOPE)
endfunction()

file(MAKE_DIRECTORY "${_deps_root}")
file(LOCK "${_extract_lock_path}" GUARD PROCESS TIMEOUT 120
  RESULT_VARIABLE _extract_lock_result)
if(NOT _extract_lock_result EQUAL 0)
  message(FATAL_ERROR
    "Unable to lock checkout-local c.pkt.systems extraction for ${_target_id}: "
    "${_extract_lock_result}")
endif()

if(DEFINED LONEJSON_C_PKT_SYSTEMS_TEST_HOLD_EXTRACTION_LOCK_SECONDS AND
   NOT "${LONEJSON_C_PKT_SYSTEMS_TEST_HOLD_EXTRACTION_LOCK_SECONDS}" STREQUAL "")
  if(NOT LONEJSON_C_PKT_SYSTEMS_TEST_HOLD_EXTRACTION_LOCK_SECONDS
     MATCHES "^[1-9][0-9]*$")
    message(FATAL_ERROR
      "LONEJSON_C_PKT_SYSTEMS_TEST_HOLD_EXTRACTION_LOCK_SECONDS must be a "
      "positive integer")
  endif()
  message(STATUS
    "c.pkt.systems: test hook holding checkout-local extraction lock for "
    "${LONEJSON_C_PKT_SYSTEMS_TEST_HOLD_EXTRACTION_LOCK_SECONDS}s")
  execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep
    "${LONEJSON_C_PKT_SYSTEMS_TEST_HOLD_EXTRACTION_LOCK_SECONDS}")
endif()

if(EXISTS "${_stamp_path}")
  file(READ "${_stamp_path}" _existing_identity)
  if(_existing_identity STREQUAL _dependency_identity)
    lonejson_validate_c_pkt_systems_bundle("${_extract_root}" "${_filename}"
      "${_target_id}" _curl_version _openssl_version)
    message(STATUS
      "c.pkt.systems ${_target_id} bundle already extracted at "
      "${_extract_root} with curl ${_curl_version} and ${_openssl_version}")
    return()
  endif()
endif()

message(STATUS "Acquiring verified c.pkt.systems archive ${_url}")
lonejson_download_c_pkt_systems_bundle("${_url}" "${_filename}" "${_expected_sha256}" _archive_path)

file(REMOVE_RECURSE "${_extract_root}")
file(REMOVE_RECURSE "${_staging_root}")
file(MAKE_DIRECTORY "${_extract_root}")
file(MAKE_DIRECTORY "${_staging_root}")

message(STATUS "Extracting ${_archive_path} to ${_extract_root}")
file(ARCHIVE_EXTRACT INPUT "${_archive_path}" DESTINATION "${_staging_root}")

if(EXISTS "${_staging_root}/${_bundle_dir_name}/include" AND
   EXISTS "${_staging_root}/${_bundle_dir_name}/lib")
  file(COPY "${_staging_root}/${_bundle_dir_name}/" DESTINATION "${_extract_root}")
elseif(EXISTS "${_staging_root}/include" AND EXISTS "${_staging_root}/lib")
  file(COPY "${_staging_root}/" DESTINATION "${_extract_root}")
else()
  file(REMOVE_RECURSE "${_staging_root}")
  message(FATAL_ERROR
    "Downloaded c.pkt.systems bundle ${_filename} does not contain the "
    "expected include/ and lib/ directories.")
endif()

lonejson_validate_c_pkt_systems_bundle("${_extract_root}" "${_filename}"
  "${_target_id}" _curl_version _openssl_version)

file(WRITE "${_stamp_path}" "${_dependency_identity}")
file(REMOVE_RECURSE "${_staging_root}")

message(STATUS
  "c.pkt.systems ${_target_id} bundle ready at ${_extract_root} with "
  "curl ${_curl_version} and ${_openssl_version}")
