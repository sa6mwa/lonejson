include_guard(GLOBAL)

function(lonejson_configure_link_warnings target)
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_link_options(${target} PRIVATE "LINKER:-fatal_warnings")
  else()
    target_link_options(${target} PRIVATE "LINKER:--fatal-warnings")
  endif()
endfunction()
