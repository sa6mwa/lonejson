if(NOT DEFINED LONEJSON_ROOT)
  message(FATAL_ERROR "LONEJSON_ROOT is required")
endif()
if(NOT DEFINED LONEJSON_PUBLIC_HEADER)
  message(FATAL_ERROR "LONEJSON_PUBLIC_HEADER is required")
endif()
if(NOT DEFINED LONEJSON_INTERNAL_IMPL)
  message(FATAL_ERROR "LONEJSON_INTERNAL_IMPL is required")
endif()
if(NOT DEFINED LONEJSON_SINGLE_HEADER_BUILD)
  message(FATAL_ERROR "LONEJSON_SINGLE_HEADER_BUILD is required")
endif()
if(NOT DEFINED LONEJSON_SINGLE_HEADER_BUILD_ALIAS)
  message(FATAL_ERROR "LONEJSON_SINGLE_HEADER_BUILD_ALIAS is required")
endif()
if(NOT DEFINED LONEJSON_SINGLE_HEADER_DIST_GZ)
  message(FATAL_ERROR "LONEJSON_SINGLE_HEADER_DIST_GZ is required")
endif()
function(lonejson_read_normalized out_var path)
  file(READ "${path}" content)
  string(REPLACE "\r\n" "\n" content "${content}")
  set(${out_var} "${content}" PARENT_SCOPE)
endfunction()

lonejson_read_normalized(public_header "${LONEJSON_PUBLIC_HEADER}")
lonejson_read_normalized(impl_header "${LONEJSON_INTERNAL_IMPL}")

string(REGEX REPLACE "\n#endif[ \t]*\n?$" ""
                     public_header_without_final_endif
                     "${public_header}")
set(single_header
    "${public_header_without_final_endif}\n\n#ifdef LONEJSON_IMPLEMENTATION\n${impl_header}#endif\n\n#endif\n")

get_filename_component(build_dir "${LONEJSON_SINGLE_HEADER_BUILD}" DIRECTORY)
get_filename_component(build_alias_dir "${LONEJSON_SINGLE_HEADER_BUILD_ALIAS}" DIRECTORY)
get_filename_component(dist_dir "${LONEJSON_SINGLE_HEADER_DIST_GZ}" DIRECTORY)
set(dist_header "${dist_dir}/lonejson.h")
file(MAKE_DIRECTORY "${build_dir}" "${build_alias_dir}" "${dist_dir}")
file(WRITE "${LONEJSON_SINGLE_HEADER_BUILD}" "${single_header}")
file(WRITE "${LONEJSON_SINGLE_HEADER_BUILD_ALIAS}" "${single_header}")
file(WRITE "${dist_header}" "${single_header}")

if(NOT DEFINED LONEJSON_CLANG_FORMAT_BIN OR LONEJSON_CLANG_FORMAT_BIN STREQUAL "")
  find_program(LONEJSON_CLANG_FORMAT_BIN NAMES clang-format)
endif()

if(DEFINED LONEJSON_CLANG_FORMAT_BIN AND NOT LONEJSON_CLANG_FORMAT_BIN STREQUAL "")
  execute_process(
    COMMAND "${LONEJSON_CLANG_FORMAT_BIN}" -i
      "${LONEJSON_SINGLE_HEADER_BUILD}"
      "${LONEJSON_SINGLE_HEADER_BUILD_ALIAS}"
      "${dist_header}"
    RESULT_VARIABLE clang_format_result
  )
  if(NOT clang_format_result EQUAL 0)
    message(FATAL_ERROR "failed to clang-format single-header artifact")
  endif()
endif()

find_program(LONEJSON_GZIP_BIN NAMES gzip)
if(NOT LONEJSON_GZIP_BIN)
  message(FATAL_ERROR "failed to find gzip for single-header artifact creation")
endif()

file(REMOVE "${LONEJSON_SINGLE_HEADER_DIST_GZ}")
execute_process(
  COMMAND "${LONEJSON_GZIP_BIN}" -9 -c "${dist_header}"
  OUTPUT_FILE "${LONEJSON_SINGLE_HEADER_DIST_GZ}"
  RESULT_VARIABLE gzip_result
)
if(NOT gzip_result EQUAL 0)
  message(FATAL_ERROR "failed to gzip single-header artifact")
endif()
file(REMOVE "${dist_header}")
