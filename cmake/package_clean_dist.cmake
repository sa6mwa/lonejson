execute_process(
  COMMAND "${LONEJSON_ROOT}/scripts/clean.sh" --dist-only --root "${LONEJSON_ROOT}"
  RESULT_VARIABLE clean_result
)
if(NOT clean_result EQUAL 0)
  message(FATAL_ERROR "failed to clean dist under ${LONEJSON_ROOT}")
endif()
