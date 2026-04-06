if(NOT DEFINED LONEJSON_BINARY_DIR)
  message(FATAL_ERROR "LONEJSON_BINARY_DIR is required")
endif()
if(NOT DEFINED LONEJSON_PUBLIC_HEADER)
  message(FATAL_ERROR "LONEJSON_PUBLIC_HEADER is required")
endif()
if(NOT DEFINED LONEJSON_SINGLE_HEADER_BUILD)
  message(FATAL_ERROR "LONEJSON_SINGLE_HEADER_BUILD is required")
endif()
if(NOT DEFINED LONEJSON_SINGLE_HEADER_BUILD_ALIAS)
  message(FATAL_ERROR "LONEJSON_SINGLE_HEADER_BUILD_ALIAS is required")
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

if(NOT EXISTS "${LONEJSON_SINGLE_HEADER_BUILD}")
  message(FATAL_ERROR
          "missing generated single-header build output: ${LONEJSON_SINGLE_HEADER_BUILD}")
endif()
if(NOT EXISTS "${LONEJSON_SINGLE_HEADER_BUILD_ALIAS}")
  message(FATAL_ERROR
          "missing generated single-header alias output: ${LONEJSON_SINGLE_HEADER_BUILD_ALIAS}")
endif()

lonejson_read_normalized(public_header "${LONEJSON_PUBLIC_HEADER}")
lonejson_read_normalized(single_header "${LONEJSON_SINGLE_HEADER_BUILD}")
lonejson_read_normalized(single_header_alias "${LONEJSON_SINGLE_HEADER_BUILD_ALIAS}")

foreach(_macro
    LONEJSON_VERSION_MAJOR
    LONEJSON_VERSION_MINOR
    LONEJSON_VERSION_PATCH
    LONEJSON_ABI_VERSION
    LJ_VERSION_MAJOR
    LJ_VERSION_MINOR
    LJ_VERSION_PATCH
    LJ_ABI_VERSION)
  lonejson_extract_macro(public_value "${public_header}" "${_macro}")
  lonejson_extract_macro(single_value "${single_header}" "${_macro}")
  lonejson_extract_macro(alias_value "${single_header_alias}" "${_macro}")
  if(NOT public_value STREQUAL single_value)
    message(FATAL_ERROR
            "generated single-header macro ${_macro} differs from public header: ${public_value} != ${single_value}")
  endif()
  if(NOT public_value STREQUAL alias_value)
    message(FATAL_ERROR
            "generated alias single-header macro ${_macro} differs from public header: ${public_value} != ${alias_value}")
  endif()
endforeach()
