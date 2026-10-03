include_guard(GLOBAL)

# Validate physical ownership before packaging creates or removes staging state.
function(lonejson_require_build_workspace workspace)
  find_program(_lonejson_workspace_bash NAMES bash REQUIRED)
  execute_process(
    COMMAND "${_lonejson_workspace_bash}"
      "${LONEJSON_ROOT}/scripts/require_build_workspace.sh" "${workspace}"
    RESULT_VARIABLE _status ERROR_VARIABLE _error)
  if(NOT _status EQUAL 0)
    message(FATAL_ERROR "Packaging workspace rejected: ${_error}")
  endif()
endfunction()
