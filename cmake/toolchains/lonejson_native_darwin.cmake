# Keep native builds native: do not set CMAKE_SYSTEM_NAME or cross-linker flags.
execute_process(
  COMMAND "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cpkt-toolchains.sh" discover arm64-apple-darwin
  RESULT_VARIABLE _apple_status OUTPUT_VARIABLE _apple_description
  ERROR_VARIABLE _apple_error)
if(NOT _apple_status EQUAL 0 OR NOT _apple_description MATCHES "source=apple")
  message(FATAL_ERROR "Native Apple toolchain discovery failed: ${_apple_error}")
endif()
foreach(_pair cc:CMAKE_C_COMPILER cxx:CMAKE_CXX_COMPILER ld:CMAKE_LINKER
    ar:CMAKE_AR ranlib:CMAKE_RANLIB strip:CMAKE_STRIP nm:CMAKE_NM
    otool:CMAKE_OTOOL install_name_tool:CMAKE_INSTALL_NAME_TOOL sysroot:CMAKE_OSX_SYSROOT)
  string(REPLACE ":" ";" _parts "${_pair}")
  list(GET _parts 0 _key)
  list(GET _parts 1 _variable)
  string(REGEX MATCH "(^|\n)${_key}=([^\r\n]+)" _match "${_apple_description}")
  if(NOT _match)
    message(FATAL_ERROR "Native Apple toolchain did not report ${_key}")
  endif()
  set(${_variable} "${CMAKE_MATCH_2}" CACHE FILEPATH "" FORCE)
endforeach()
set(CMAKE_OSX_ARCHITECTURES arm64 CACHE STRING "" FORCE)
set(LONEJSON_MACOS_DEPLOYMENT_TARGET "15.0" CACHE STRING "Minimum macOS deployment target")
set(CMAKE_OSX_DEPLOYMENT_TARGET "${LONEJSON_MACOS_DEPLOYMENT_TARGET}" CACHE STRING "" FORCE)
set(LONEJSON_TARGET_ARCH arm64 CACHE STRING "" FORCE)
set(LONEJSON_TARGET_OS darwin CACHE STRING "" FORCE)
set(LONEJSON_TARGET_LIBC "" CACHE STRING "" FORCE)
