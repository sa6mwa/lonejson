if(NOT DEFINED LONEJSON_SONAME_FILE_NAME)
  message(FATAL_ERROR "LONEJSON_SONAME_FILE_NAME is required")
endif()

if(NOT DEFINED LONEJSON_ABI_VERSION)
  message(FATAL_ERROR "LONEJSON_ABI_VERSION is required")
endif()

if(NOT LONEJSON_SONAME_FILE_NAME MATCHES "^liblonejson\\.so\\.[0-9]+$")
  message(FATAL_ERROR
          "unexpected SONAME file name: ${LONEJSON_SONAME_FILE_NAME}")
endif()

if(NOT LONEJSON_SONAME_FILE_NAME STREQUAL "liblonejson.so.${LONEJSON_ABI_VERSION}")
  message(FATAL_ERROR
          "expected SONAME liblonejson.so.${LONEJSON_ABI_VERSION}, got ${LONEJSON_SONAME_FILE_NAME}")
endif()
