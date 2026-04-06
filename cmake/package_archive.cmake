set(package_root "${LONEJSON_BINARY_DIR}/package/archive")
file(REMOVE_RECURSE "${package_root}")
file(MAKE_DIRECTORY "${package_root}/include")
file(MAKE_DIRECTORY "${package_root}/lib")
file(MAKE_DIRECTORY "${package_root}/share/doc/liblonejson")

file(COPY "${LONEJSON_PUBLIC_HEADER}" DESTINATION "${package_root}/include")
file(COPY "${LONEJSON_SHARED_LIB}" DESTINATION "${package_root}/lib")
file(COPY "${LONEJSON_STATIC_LIB}" DESTINATION "${package_root}/lib")

if(DEFINED LONEJSON_SHARED_LINK_NAME
   AND NOT LONEJSON_SHARED_LINK_NAME STREQUAL ""
   AND NOT LONEJSON_SHARED_LINK_NAME STREQUAL LONEJSON_SHARED_LIB_NAME)
  file(CREATE_LINK "${LONEJSON_SHARED_LIB_NAME}"
    "${package_root}/lib/${LONEJSON_SHARED_LINK_NAME}"
    SYMBOLIC)
endif()

if(DEFINED LONEJSON_SHARED_SONAME
   AND DEFINED LONEJSON_SHARED_LIB_NAME
   AND NOT LONEJSON_SHARED_SONAME STREQUAL ""
   AND NOT LONEJSON_SHARED_SONAME STREQUAL LONEJSON_SHARED_LIB_NAME)
  file(CREATE_LINK "${LONEJSON_SHARED_LIB_NAME}"
    "${package_root}/lib/${LONEJSON_SHARED_SONAME}"
    SYMBOLIC)
endif()

file(COPY "${LONEJSON_ROOT}/LICENSE" DESTINATION "${package_root}/share/doc/liblonejson")
file(COPY "${LONEJSON_ROOT}/README.md" DESTINATION "${package_root}/share/doc/liblonejson")

file(MAKE_DIRECTORY "${LONEJSON_ROOT}/dist")
if(NOT DEFINED LONEJSON_RELEASE_VARIANT_SUFFIX)
  set(LONEJSON_RELEASE_VARIANT_SUFFIX "")
endif()
set(archive_base "${LONEJSON_ROOT}/dist/liblonejson-${LONEJSON_VERSION}-${LONEJSON_TARGET_ID}${LONEJSON_RELEASE_VARIANT_SUFFIX}.tar")
set(archive "${archive_base}.gz")

find_program(LONEJSON_TAR_BIN NAMES tar)
find_program(LONEJSON_GZIP_BIN NAMES gzip)
if(NOT LONEJSON_TAR_BIN)
  message(FATAL_ERROR "failed to find tar for archive creation")
endif()
if(NOT LONEJSON_GZIP_BIN)
  message(FATAL_ERROR "failed to find gzip for archive creation")
endif()

file(REMOVE "${archive_base}" "${archive}")
execute_process(
  COMMAND "${LONEJSON_TAR_BIN}" -cf "${archive_base}" --format=gnu --owner=0 --group=0 .
  WORKING_DIRECTORY "${package_root}"
  RESULT_VARIABLE tar_result
)
if(NOT tar_result EQUAL 0)
  message(FATAL_ERROR "failed to create package tar archive")
endif()

execute_process(
  COMMAND "${LONEJSON_GZIP_BIN}" -9 -f "${archive_base}"
  RESULT_VARIABLE gzip_result
)
if(NOT gzip_result EQUAL 0)
  message(FATAL_ERROR "failed to gzip package archive")
endif()
