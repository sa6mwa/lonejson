if(NOT DEFINED LONEJSON_BINARY_DIR)
  message(FATAL_ERROR "LONEJSON_BINARY_DIR is required")
endif()
if(NOT DEFINED LONEJSON_RELEASE_VERSION)
  message(FATAL_ERROR "LONEJSON_RELEASE_VERSION is required")
endif()
if(NOT DEFINED LONEJSON_SINGLE_HEADER_DIST_GZ)
  message(FATAL_ERROR "LONEJSON_SINGLE_HEADER_DIST_GZ is required")
endif()

function(lonejson_read_normalized out_var path)
  file(READ "${path}" content)
  string(REPLACE "\r\n" "\n" content "${content}")
  set(${out_var} "${content}" PARENT_SCOPE)
endfunction()

function(lonejson_extract_macro out_var content macro_name)
  string(REGEX MATCH "#define[ \t]+${macro_name}[ \t]+([^ \t\r\n]+)"
         _match "${content}")
  if(NOT _match)
    message(FATAL_ERROR "missing macro ${macro_name}")
  endif()
  set(${out_var} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${LONEJSON_BINARY_DIR}"
          --target package-single-header
  RESULT_VARIABLE build_status
  OUTPUT_VARIABLE build_out
  ERROR_VARIABLE build_err)
if(NOT build_status EQUAL 0)
  message(FATAL_ERROR
          "package-single-header build failed:\n${build_out}\n${build_err}")
endif()

if(NOT EXISTS "${LONEJSON_SINGLE_HEADER_DIST_GZ}")
  message(FATAL_ERROR
          "missing generated single-header release artifact: ${LONEJSON_SINGLE_HEADER_DIST_GZ}")
endif()

find_program(LONEJSON_GZIP_BIN NAMES gzip)
if(NOT LONEJSON_GZIP_BIN)
  message(FATAL_ERROR "failed to find gzip for release header inspection")
endif()

execute_process(
  COMMAND "${LONEJSON_GZIP_BIN}" -cd "${LONEJSON_SINGLE_HEADER_DIST_GZ}"
  RESULT_VARIABLE gzip_status
  OUTPUT_VARIABLE dist_header
  ERROR_VARIABLE gzip_err)
if(NOT gzip_status EQUAL 0)
  message(FATAL_ERROR
          "failed to decompress release header:\n${gzip_err}")
endif()

string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$" _version_match
       "${LONEJSON_RELEASE_VERSION}")
if(NOT _version_match)
  message(FATAL_ERROR
          "LONEJSON_RELEASE_VERSION must be in X.Y.Z form, got ${LONEJSON_RELEASE_VERSION}")
endif()
set(release_version_major "${CMAKE_MATCH_1}")
set(release_version_minor "${CMAKE_MATCH_2}")
set(release_version_patch "${CMAKE_MATCH_3}")

foreach(_macro_pair
    "LONEJSON_VERSION_MAJOR;${release_version_major}"
    "LONEJSON_VERSION_MINOR;${release_version_minor}"
    "LONEJSON_VERSION_PATCH;${release_version_patch}"
    "LJ_VERSION_MAJOR;${release_version_major}"
    "LJ_VERSION_MINOR;${release_version_minor}"
    "LJ_VERSION_PATCH;${release_version_patch}")
  string(REPLACE ";" "|" _macro_pair_text "${_macro_pair}")
  string(REPLACE "|" ";" _macro_pair_parts "${_macro_pair_text}")
  list(GET _macro_pair_parts 0 _macro)
  list(GET _macro_pair_parts 1 _expected)
  lonejson_extract_macro(_actual "${dist_header}" "${_macro}")
  if(NOT _actual STREQUAL _expected)
    message(FATAL_ERROR
            "release header macro ${_macro} expected ${_expected}, got ${_actual}")
  endif()
endforeach()
