if(NOT LONEJSON_BUILD_WITH_CURL)
  message(FATAL_ERROR
    "package-archive requires LONEJSON_BUILD_WITH_CURL=ON; binary releases "
    "must include the lonejson_curl_* ABI")
endif()

set(archive_name "liblonejson-${LONEJSON_VERSION}-${LONEJSON_TARGET_ID}")
set(package_stage_root "${LONEJSON_BINARY_DIR}/package/archive")
set(package_root "${package_stage_root}/${archive_name}")
file(REMOVE_RECURSE "${package_stage_root}")
include("${LONEJSON_ROOT}/cmake/c_pkt_systems_metadata.cmake")
lonejson_c_pkt_systems_sha256("${LONEJSON_TARGET_ID}" c_pkt_systems_sha256)
set(c_pkt_systems_url
    "${LONEJSON_C_PKT_SYSTEMS_BASE_URL_DEFAULT}/c.pkt.systems-${LONEJSON_C_PKT_SYSTEMS_PINNED_VERSION}-${LONEJSON_TARGET_ID}.tar.gz")
file(MAKE_DIRECTORY "${package_root}/include")
file(MAKE_DIRECTORY "${package_root}/lib")
file(MAKE_DIRECTORY "${package_root}/lib/cmake/lonejson")
file(MAKE_DIRECTORY "${package_root}/lib/pkgconfig")
file(MAKE_DIRECTORY "${package_root}/share/lonejson")
file(MAKE_DIRECTORY "${package_root}/share/doc/liblonejson")

file(COPY "${LONEJSON_PUBLIC_HEADER}" DESTINATION "${package_root}/include")
file(COPY "${LONEJSON_SHARED_LIB}" DESTINATION "${package_root}/lib")
file(COPY "${LONEJSON_STATIC_LIB}" DESTINATION "${package_root}/lib")

set(packaged_shared_lib "${package_root}/lib/${LONEJSON_SHARED_LIB_NAME}")
set(packaged_static_lib "${package_root}/lib/${LONEJSON_STATIC_LIB_NAME}")
if(DEFINED LONEJSON_STRIP AND NOT LONEJSON_STRIP STREQUAL "")
  if(NOT LONEJSON_TARGET_ID MATCHES "darwin")
    set(strip_shared_flag "--strip-unneeded")
    set(strip_static_flag "--strip-debug")
    execute_process(
      COMMAND "${LONEJSON_STRIP}" "${strip_shared_flag}" "${packaged_shared_lib}"
      RESULT_VARIABLE strip_shared_result
    )
    if(NOT strip_shared_result EQUAL 0)
      message(FATAL_ERROR "failed to strip ${packaged_shared_lib}")
    endif()
    execute_process(
      COMMAND "${LONEJSON_STRIP}" "${strip_static_flag}" "${packaged_static_lib}"
      RESULT_VARIABLE strip_static_result
    )
    if(NOT strip_static_result EQUAL 0)
      message(FATAL_ERROR "failed to strip ${packaged_static_lib}")
    endif()
  endif()
endif()

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

set(dependencies_file "${package_root}/share/lonejson/dependencies.json")
file(WRITE "${dependencies_file}"
"{
  \"schema\": \"pkt.systems.dependencies.v1\",
  \"project\": \"lonejson\",
  \"version\": \"${LONEJSON_VERSION}\",
  \"target_id\": \"${LONEJSON_TARGET_ID}\",
  \"dependencies\": [
    {
      \"name\": \"c.pkt.systems\",
      \"version\": \"${LONEJSON_C_PKT_SYSTEMS_PINNED_VERSION}\",
      \"target_id\": \"${LONEJSON_TARGET_ID}\",
      \"source_url\": \"${c_pkt_systems_url}\",
      \"sha256\": \"${c_pkt_systems_sha256}\",
      \"bundled\": false,
      \"external\": true,
      \"role\": \"release-sdk-build-dependency\",
      \"provides\": [
        \"curl\",
        \"openssl\"
      ]
    }
  ]
}
")

set(pkgconfig_file "${package_root}/lib/pkgconfig/lonejson.pc")
file(WRITE "${pkgconfig_file}"
"prefix=\${pcfiledir}/../..
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: lonejson
Description: Strict C89 JSON parser, serializer, and streaming toolkit
Version: ${LONEJSON_VERSION}
Libs: -L\${libdir} -llonejson
Cflags: -I\${includedir}
")

set(cmake_config_file "${package_root}/lib/cmake/lonejson/lonejsonConfig.cmake")
file(WRITE "${cmake_config_file}"
"get_filename_component(_lonejson_prefix \"\${CMAKE_CURRENT_LIST_DIR}/../../..\" ABSOLUTE)

if(NOT TARGET lonejson::lonejson)
  add_library(lonejson::lonejson SHARED IMPORTED)
  set_target_properties(lonejson::lonejson PROPERTIES
    IMPORTED_LOCATION \"\${_lonejson_prefix}/lib/${LONEJSON_SHARED_LIB_NAME}\"
    INTERFACE_INCLUDE_DIRECTORIES \"\${_lonejson_prefix}/include\"
  )
endif()

if(NOT TARGET lonejson::lonejson_static)
  add_library(lonejson::lonejson_static STATIC IMPORTED)
  set_target_properties(lonejson::lonejson_static PROPERTIES
    IMPORTED_LOCATION \"\${_lonejson_prefix}/lib/${LONEJSON_STATIC_LIB_NAME}\"
    INTERFACE_INCLUDE_DIRECTORIES \"\${_lonejson_prefix}/include\"
  )
endif()

set(lonejson_FOUND TRUE)
unset(_lonejson_prefix)
")

set(cmake_version_file "${package_root}/lib/cmake/lonejson/lonejsonConfigVersion.cmake")
file(WRITE "${cmake_version_file}"
"set(PACKAGE_VERSION \"${LONEJSON_VERSION}\")

if(PACKAGE_FIND_VERSION)
  if(PACKAGE_FIND_VERSION VERSION_EQUAL PACKAGE_VERSION)
    set(PACKAGE_VERSION_EXACT TRUE)
    set(PACKAGE_VERSION_COMPATIBLE TRUE)
  elseif(PACKAGE_FIND_VERSION VERSION_LESS PACKAGE_VERSION)
    set(PACKAGE_VERSION_COMPATIBLE TRUE)
  else()
    set(PACKAGE_VERSION_COMPATIBLE FALSE)
  endif()
endif()
")

file(MAKE_DIRECTORY "${LONEJSON_ROOT}/dist")
set(archive_base "${LONEJSON_ROOT}/dist/${archive_name}.tar")
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
  COMMAND "${LONEJSON_TAR_BIN}" -cf "${archive_base}" --format=gnu --owner=0 --group=0 "${archive_name}"
  WORKING_DIRECTORY "${package_stage_root}"
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
