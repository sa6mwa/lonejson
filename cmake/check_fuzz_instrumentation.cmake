if(NOT DEFINED LONEJSON_COMPILE_COMMANDS)
  message(FATAL_ERROR "LONEJSON_COMPILE_COMMANDS is required")
endif()
if(NOT DEFINED LONEJSON_SOURCE_FILE)
  message(FATAL_ERROR "LONEJSON_SOURCE_FILE is required")
endif()

file(READ "${LONEJSON_COMPILE_COMMANDS}" LONEJSON_COMPILE_COMMANDS_JSON)

string(JSON LONEJSON_COMMAND_COUNT LENGTH "${LONEJSON_COMPILE_COMMANDS_JSON}")
math(EXPR LONEJSON_LAST_COMMAND "${LONEJSON_COMMAND_COUNT} - 1")
set(LONEJSON_FOUND_SOURCE OFF)

foreach(LONEJSON_INDEX RANGE 0 ${LONEJSON_LAST_COMMAND})
  string(JSON LONEJSON_COMMAND_FILE GET "${LONEJSON_COMPILE_COMMANDS_JSON}"
         ${LONEJSON_INDEX} file)
  if(LONEJSON_COMMAND_FILE STREQUAL LONEJSON_SOURCE_FILE)
    set(LONEJSON_FOUND_SOURCE ON)
    string(JSON LONEJSON_COMMAND GET "${LONEJSON_COMPILE_COMMANDS_JSON}"
           ${LONEJSON_INDEX} command)
    string(FIND "${LONEJSON_COMMAND}"
                "-fsanitize=fuzzer-no-link,address,undefined"
                LONEJSON_SANITIZER_FLAG_INDEX)
    string(FIND "${LONEJSON_COMMAND}" "-fno-omit-frame-pointer"
                LONEJSON_FRAME_POINTER_FLAG_INDEX)
    if(NOT LONEJSON_SANITIZER_FLAG_INDEX LESS 0 AND
       NOT LONEJSON_FRAME_POINTER_FLAG_INDEX LESS 0)
      return()
    endif()
  endif()
endforeach()

if(NOT LONEJSON_FOUND_SOURCE)
  message(FATAL_ERROR
          "compile_commands.json does not contain ${LONEJSON_SOURCE_FILE}")
endif()

message(FATAL_ERROR
        "fuzz build does not contain an instrumented ${LONEJSON_SOURCE_FILE}; "
        "expected -fsanitize=fuzzer-no-link,address,undefined and "
        "-fno-omit-frame-pointer")
