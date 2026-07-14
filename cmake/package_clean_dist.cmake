if(NOT DEFINED LONEJSON_DIST_DIR OR LONEJSON_DIST_DIR STREQUAL "")
  set(LONEJSON_DIST_DIR "${LONEJSON_ROOT}/dist")
endif()
get_filename_component(LONEJSON_DIST_DIR "${LONEJSON_DIST_DIR}" ABSOLUTE)

execute_process(
  COMMAND "${LONEJSON_ROOT}/scripts/clean.sh"
    --dist-only --root "${LONEJSON_ROOT}" --dist-dir "${LONEJSON_DIST_DIR}"
  RESULT_VARIABLE clean_result
)
if(NOT clean_result EQUAL 0)
  message(FATAL_ERROR "failed to clean dist at ${LONEJSON_DIST_DIR}")
endif()
