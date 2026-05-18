if(NOT DEFINED LONEJSON_PUBLIC_HEADER)
  message(FATAL_ERROR "LONEJSON_PUBLIC_HEADER is required")
endif()
if(NOT DEFINED LONEJSON_ABI_VERSION)
  message(FATAL_ERROR "LONEJSON_ABI_VERSION is required")
endif()

file(READ "${LONEJSON_PUBLIC_HEADER}" header_content)
string(REGEX MATCH "#define[ \t]+LONEJSON_ABI_VERSION[ \t]+([0-9]+)"
       _abi_match "${header_content}")
if(NOT _abi_match)
  message(FATAL_ERROR "missing LONEJSON_ABI_VERSION in public header")
endif()

set(header_abi_version "${CMAKE_MATCH_1}")
if(NOT header_abi_version STREQUAL "${LONEJSON_ABI_VERSION}")
  message(FATAL_ERROR
          "public header ABI version differs from CMake ABI version: ${header_abi_version} != ${LONEJSON_ABI_VERSION}")
endif()
