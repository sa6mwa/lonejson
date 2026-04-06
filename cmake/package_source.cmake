set(dist_dir "${LONEJSON_ROOT}/dist")
set(source_root_name "lonejson-${LONEJSON_VERSION}")
set(archive_base "${dist_dir}/${source_root_name}.tar")
set(archive_path "${archive_base}.gz")
set(stage_root "${dist_dir}/.source-pack")
set(stage_dir "${stage_root}/${source_root_name}")

find_program(LONEJSON_GZIP_BIN NAMES gzip)
if(NOT LONEJSON_GZIP_BIN)
  message(FATAL_ERROR "failed to find gzip for source archive creation")
endif()

file(MAKE_DIRECTORY "${dist_dir}")
file(REMOVE_RECURSE "${stage_root}")
file(REMOVE "${archive_base}" "${archive_path}")
file(MAKE_DIRECTORY "${stage_root}")

execute_process(
  COMMAND "${LONEJSON_ROOT}/scripts/stage_release_sources.sh"
          "${LONEJSON_ROOT}" "${stage_dir}" "${LONEJSON_VERSION}"
  RESULT_VARIABLE stage_result
)
if(NOT stage_result EQUAL 0)
  file(REMOVE_RECURSE "${stage_root}")
  message(FATAL_ERROR "failed to stage source archive tree")
endif()

execute_process(
  COMMAND tar -cf "${archive_base}" "${source_root_name}"
  WORKING_DIRECTORY "${stage_root}"
  RESULT_VARIABLE archive_result
)
if(NOT archive_result EQUAL 0)
  file(REMOVE_RECURSE "${stage_root}")
  message(FATAL_ERROR "failed to create source archive")
endif()

execute_process(
  COMMAND "${LONEJSON_GZIP_BIN}" -9 -f "${archive_base}"
  RESULT_VARIABLE gzip_result
)
if(NOT gzip_result EQUAL 0)
  file(REMOVE_RECURSE "${stage_root}")
  message(FATAL_ERROR "failed to gzip source archive")
endif()
file(REMOVE_RECURSE "${stage_root}")
