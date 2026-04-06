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

get_filename_component(impl_dir "${LONEJSON_INTERNAL_IMPL}" DIRECTORY)
set(impl_parts_dir "${impl_dir}/impl")
if(EXISTS "${impl_parts_dir}")
  file(GLOB impl_parts RELATIVE "${LONEJSON_ROOT}" "${impl_parts_dir}/*.h")
  list(SORT impl_parts)
  set(impl_header "")
  foreach(part IN LISTS impl_parts)
    lonejson_read_normalized(part_content "${LONEJSON_ROOT}/${part}")
    string(APPEND impl_header "${part_content}")
    if(NOT part_content MATCHES "\n$")
      string(APPEND impl_header "\n")
    endif()
  endforeach()
else()
  lonejson_read_normalized(impl_header "${LONEJSON_INTERNAL_IMPL}")
endif()

string(REGEX REPLACE "\n#endif[ \t]*\n?$" ""
                     public_header_without_final_endif
                     "${public_header}")
set(single_header
    "${public_header_without_final_endif}\n\n#ifdef LONEJSON_IMPLEMENTATION\n${impl_header}#endif\n\n#endif\n")

get_filename_component(build_dir "${LONEJSON_SINGLE_HEADER_BUILD}" DIRECTORY)
get_filename_component(build_alias_dir "${LONEJSON_SINGLE_HEADER_BUILD_ALIAS}" DIRECTORY)
get_filename_component(dist_dir "${LONEJSON_SINGLE_HEADER_DIST_GZ}" DIRECTORY)
set(dist_header "${dist_dir}/lonejson.h")
set(build_tmp "${build_dir}/lonejson_single_header.h.tmp")
set(build_alias_tmp "${build_alias_dir}/lonejson.h.tmp")
set(dist_header_tmp "${dist_dir}/lonejson.h.tmp")
file(MAKE_DIRECTORY "${build_dir}" "${build_alias_dir}" "${dist_dir}")
file(WRITE "${build_tmp}" "${single_header}")
file(WRITE "${build_alias_tmp}" "${single_header}")
file(WRITE "${dist_header_tmp}" "${single_header}")

if(NOT DEFINED LONEJSON_CLANG_FORMAT_BIN OR LONEJSON_CLANG_FORMAT_BIN STREQUAL "")
  find_program(LONEJSON_CLANG_FORMAT_BIN NAMES clang-format)
endif()

if(DEFINED LONEJSON_CLANG_FORMAT_BIN AND NOT LONEJSON_CLANG_FORMAT_BIN STREQUAL "")
  execute_process(
    COMMAND "${LONEJSON_CLANG_FORMAT_BIN}" -i
      "${build_tmp}"
      "${build_alias_tmp}"
      "${dist_header_tmp}"
    RESULT_VARIABLE clang_format_result
  )
  if(NOT clang_format_result EQUAL 0)
    message(FATAL_ERROR "failed to clang-format single-header artifact")
  endif()
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${build_tmp}" "${LONEJSON_SINGLE_HEADER_BUILD}"
  RESULT_VARIABLE copy_build_result
)
if(NOT copy_build_result EQUAL 0)
  message(FATAL_ERROR "failed to update build single-header artifact")
endif()
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${build_alias_tmp}" "${LONEJSON_SINGLE_HEADER_BUILD_ALIAS}"
  RESULT_VARIABLE copy_alias_result
)
if(NOT copy_alias_result EQUAL 0)
  message(FATAL_ERROR "failed to update build alias single-header artifact")
endif()
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${dist_header_tmp}" "${dist_header}"
  RESULT_VARIABLE copy_dist_result
)
if(NOT copy_dist_result EQUAL 0)
  message(FATAL_ERROR "failed to update dist single-header artifact")
endif()
file(REMOVE "${build_tmp}" "${build_alias_tmp}" "${dist_header_tmp}")

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
