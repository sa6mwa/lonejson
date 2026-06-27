cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED LONEJSON_SOURCE_DIR)
  get_filename_component(LONEJSON_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()

set(LONEJSON_C_PKT_SYSTEMS_PINNED_VERSION "0.5.0")
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
    "https://github.com/sa6mwa/c.pkt.systems/releases/download/v${LONEJSON_C_PKT_SYSTEMS_VERSION}"
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
    message(FATAL_ERROR
      "Unable to detect whether host libc is glibc or musl. Override with "
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

function(lonejson_c_pkt_systems_sha256 target_id out_var)
  if(target_id STREQUAL "x86_64-linux-gnu")
    set(_sha256 "737e1bc3992f00e9d4c71c6cae87db8dce669ce795b42ba769e686ff263cfaea")
  elseif(target_id STREQUAL "x86_64-linux-musl")
    set(_sha256 "435c25dfa05e831c5183ed0628730749625aa68804e21a3615937d36d11b7384")
  elseif(target_id STREQUAL "aarch64-linux-gnu")
    set(_sha256 "e7163bea53ec8b72bd88b6387289eddc6d11383e8903ae756b06aea03853ee9d")
  elseif(target_id STREQUAL "aarch64-linux-musl")
    set(_sha256 "ee19c298dffe5b9b3d304d416de95a897c547f9489eb52493500576c0d36830f")
  elseif(target_id STREQUAL "armhf-linux-gnu")
    set(_sha256 "b6f12112207232ef16123d19535d36d73c9a0c75fdd9c1566684f2f36b384d94")
  elseif(target_id STREQUAL "armhf-linux-musl")
    set(_sha256 "2de19d4fb76379a7585382ac6b77d3d9a70c505f8ef9334a5ff531336c84b368")
  elseif(target_id STREQUAL "arm64-apple-darwin")
    set(_sha256 "ebf1caf096df8c17067df2bd02efe65793ad98f65dafb5f29384ce6a6269a829")
  else()
    message(FATAL_ERROR "No pinned c.pkt.systems checksum for ${target_id}")
  endif()
  set(${out_var} "${_sha256}" PARENT_SCOPE)
endfunction()

function(lonejson_download_c_pkt_systems_bundle url archive_path expected_sha256)
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

  set(_last_message "download not attempted")
  foreach(_attempt RANGE 1 ${LONEJSON_C_PKT_SYSTEMS_DOWNLOAD_RETRIES})
    file(REMOVE "${archive_path}")

    if(DEFINED LONEJSON_C_PKT_SYSTEMS_TEST_FAIL_DOWNLOAD_ATTEMPTS AND
       _attempt LESS_EQUAL LONEJSON_C_PKT_SYSTEMS_TEST_FAIL_DOWNLOAD_ATTEMPTS)
      set(_download_code 22)
      set(_download_message "HTTP response code said error")
    else()
      file(DOWNLOAD
        "${url}"
        "${archive_path}"
        SHOW_PROGRESS
        STATUS _download_status
        TLS_VERIFY ON)
      list(GET _download_status 0 _download_code)
      list(GET _download_status 1 _download_message)
    endif()

    if(_download_code EQUAL 0)
      file(SHA256 "${archive_path}" _downloaded_sha256)
      if(_downloaded_sha256 STREQUAL expected_sha256)
        return()
      endif()
      set(_download_code 1)
      set(_download_message
        "SHA256 mismatch (expected ${expected_sha256}, got ${_downloaded_sha256})")
    endif()

    file(REMOVE "${archive_path}")
    set(_last_message "${_download_message}")
    if(_attempt LESS LONEJSON_C_PKT_SYSTEMS_DOWNLOAD_RETRIES)
      message(WARNING
        "Download attempt ${_attempt}/${LONEJSON_C_PKT_SYSTEMS_DOWNLOAD_RETRIES} "
        "failed for ${url}: ${_download_message}; retrying in "
        "${LONEJSON_C_PKT_SYSTEMS_RETRY_DELAY_SECONDS}s")
      if(NOT LONEJSON_C_PKT_SYSTEMS_RETRY_DELAY_SECONDS STREQUAL "0")
        execute_process(COMMAND "${CMAKE_COMMAND}" -E
          sleep "${LONEJSON_C_PKT_SYSTEMS_RETRY_DELAY_SECONDS}")
      endif()
    endif()
  endforeach()

  message(FATAL_ERROR
    "Failed to download ${url} after "
    "${LONEJSON_C_PKT_SYSTEMS_DOWNLOAD_RETRIES} attempts: ${_last_message}")
endfunction()

set(_target_id "${LONEJSON_C_PKT_SYSTEMS_TARGET_ID}")
set(_bundle_dir_name "c.pkt.systems-${LONEJSON_C_PKT_SYSTEMS_VERSION}-${_target_id}")
set(_filename "${_bundle_dir_name}.tar.gz")
set(_url "${LONEJSON_C_PKT_SYSTEMS_BASE_URL}/${_filename}")
set(_deps_root "${LONEJSON_SOURCE_DIR}/.deps/c.pkt.systems/${_target_id}")
set(_archive_path "${_deps_root}/${_filename}")
set(_extract_root "${_deps_root}/root")
set(_staging_root "${_deps_root}/extract")
set(_stamp_path "${_extract_root}/.lonejson-c-pkt-systems-version")
lonejson_c_pkt_systems_sha256("${_target_id}" _expected_sha256)
if(DEFINED LONEJSON_C_PKT_SYSTEMS_EXPECTED_SHA256_OVERRIDE)
  set(_expected_sha256 "${LONEJSON_C_PKT_SYSTEMS_EXPECTED_SHA256_OVERRIDE}")
endif()

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

if(EXISTS "${_stamp_path}")
  file(READ "${_stamp_path}" _existing_version)
  string(STRIP "${_existing_version}" _existing_version)
  if(_existing_version STREQUAL "${LONEJSON_C_PKT_SYSTEMS_VERSION}")
    lonejson_validate_c_pkt_systems_bundle("${_extract_root}" "${_filename}"
      "${_target_id}" _curl_version _openssl_version)
    message(STATUS
      "c.pkt.systems ${_target_id} bundle already extracted at "
      "${_extract_root} with curl ${_curl_version} and ${_openssl_version}")
    return()
  endif()
endif()

message(STATUS "Downloading ${_url}")
lonejson_download_c_pkt_systems_bundle("${_url}" "${_archive_path}" "${_expected_sha256}")

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

file(WRITE "${_stamp_path}" "${LONEJSON_C_PKT_SYSTEMS_VERSION}\n")
file(REMOVE_RECURSE "${_staging_root}")

message(STATUS
  "c.pkt.systems ${_target_id} bundle ready at ${_extract_root} with "
  "curl ${_curl_version} and ${_openssl_version}")
