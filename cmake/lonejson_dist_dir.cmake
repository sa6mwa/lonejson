# Prepare an artifact directory without taking ownership of an arbitrary path.
#
# The repository's default dist/ is always safe. A custom destination is
# lifecycle-owned only when it is newly created here or carries our marker;
# this keeps package targets and package-clean-dist on the same safety model.
if(NOT DEFINED LONEJSON_ROOT OR LONEJSON_ROOT STREQUAL "")
  message(FATAL_ERROR "LONEJSON_ROOT is required to prepare the artifact directory")
endif()

get_filename_component(LONEJSON_ROOT "${LONEJSON_ROOT}" ABSOLUTE)
if(NOT DEFINED LONEJSON_DIST_DIR OR LONEJSON_DIST_DIR STREQUAL "")
  set(LONEJSON_DIST_DIR "${LONEJSON_ROOT}/dist")
endif()
get_filename_component(LONEJSON_DIST_DIR "${LONEJSON_DIST_DIR}" ABSOLUTE)

function(lonejson_prepare_dist_dir)
  set(_lonejson_default_dist "${LONEJSON_ROOT}/dist")
  if(LONEJSON_DIST_DIR STREQUAL _lonejson_default_dist)
    file(MAKE_DIRECTORY "${LONEJSON_DIST_DIR}")
    return()
  endif()

  file(RELATIVE_PATH _lonejson_dist_relative
    "${LONEJSON_ROOT}" "${LONEJSON_DIST_DIR}")
  if(_lonejson_dist_relative STREQUAL "." OR
      NOT _lonejson_dist_relative MATCHES "^\\.\\.")
    message(FATAL_ERROR
      "refusing to use project directory as custom dist: ${LONEJSON_DIST_DIR}")
  endif()

  if(EXISTS "${LONEJSON_DIST_DIR}" AND
      NOT EXISTS "${LONEJSON_DIST_DIR}/.lonejson-dist")
    message(FATAL_ERROR
      "refusing to use unmarked custom dist directory: ${LONEJSON_DIST_DIR}")
  endif()

  file(MAKE_DIRECTORY "${LONEJSON_DIST_DIR}")
  file(WRITE "${LONEJSON_DIST_DIR}/.lonejson-dist"
    "lonejson lifecycle artifact directory\n")
endfunction()
