if(NOT DEFINED LONEJSON_BENCH_EXE)
  message(FATAL_ERROR "LONEJSON_BENCH_EXE is required")
endif()

set(_tmp_dir "${CMAKE_CURRENT_BINARY_DIR}/bench-gate-check")
file(REMOVE_RECURSE "${_tmp_dir}")
file(MAKE_DIRECTORY "${_tmp_dir}")

set(_baseline "${_tmp_dir}/baseline.json")
set(_latest_mixed "${_tmp_dir}/latest-mixed.json")
set(_latest_improve "${_tmp_dir}/latest-improve.json")
set(_latest_broken "${_tmp_dir}/latest-broken.json")

set(_common_prefix [=[
{"schema_version":12,"timestamp_epoch_ns":1,"timestamp_utc":"2026-04-05T00:00:00Z","host":"test","compiler":"test","corpus_path":"test","corpus_file_count":1,"corpus_total_bytes":1,"corpus_y_count":1,"corpus_n_count":0,"corpus_i_count":0,"iterations":100,"parser_buffer_size":4096,"push_parser_buffer_size":4096,"reader_buffer_size":1024,"stream_buffer_size":1024,"results":[
]=])
set(_common_suffix "]}")

file(WRITE "${_baseline}"
  "${_common_prefix}"
  "{\"name\":\"bench/a\",\"group\":\"test\",\"elapsed_ns\":1,\"total_bytes\":1,\"total_documents\":1,\"mismatch_count\":0,\"mib_per_sec\":100.0,\"docs_per_sec\":100.0,\"ns_per_byte\":1.0},"
  "{\"name\":\"bench/b\",\"group\":\"test\",\"elapsed_ns\":1,\"total_bytes\":1,\"total_documents\":1,\"mismatch_count\":0,\"mib_per_sec\":100.0,\"docs_per_sec\":100.0,\"ns_per_byte\":1.0},"
  "{\"name\":\"bench/c\",\"group\":\"test\",\"elapsed_ns\":1,\"total_bytes\":1,\"total_documents\":1,\"mismatch_count\":0,\"mib_per_sec\":100.0,\"docs_per_sec\":100.0,\"ns_per_byte\":1.0},"
  "{\"name\":\"bench/d\",\"group\":\"test\",\"elapsed_ns\":1,\"total_bytes\":1,\"total_documents\":1,\"mismatch_count\":0,\"mib_per_sec\":100.0,\"docs_per_sec\":100.0,\"ns_per_byte\":1.0}"
  "${_common_suffix}")

file(WRITE "${_latest_mixed}"
  "${_common_prefix}"
  "{\"name\":\"bench/a\",\"group\":\"test\",\"elapsed_ns\":1,\"total_bytes\":1,\"total_documents\":1,\"mismatch_count\":0,\"mib_per_sec\":96.0,\"docs_per_sec\":96.0,\"ns_per_byte\":1.0},"
  "{\"name\":\"bench/b\",\"group\":\"test\",\"elapsed_ns\":1,\"total_bytes\":1,\"total_documents\":1,\"mismatch_count\":0,\"mib_per_sec\":94.0,\"docs_per_sec\":94.0,\"ns_per_byte\":1.0},"
  "{\"name\":\"bench/c\",\"group\":\"test\",\"elapsed_ns\":1,\"total_bytes\":1,\"total_documents\":1,\"mismatch_count\":0,\"mib_per_sec\":106.0,\"docs_per_sec\":106.0,\"ns_per_byte\":1.0},"
  "{\"name\":\"bench/d\",\"group\":\"test\",\"elapsed_ns\":1,\"total_bytes\":1,\"total_documents\":1,\"mismatch_count\":0,\"mib_per_sec\":112.0,\"docs_per_sec\":112.0,\"ns_per_byte\":1.0}"
  "${_common_suffix}")

file(WRITE "${_latest_improve}"
  "${_common_prefix}"
  "{\"name\":\"bench/a\",\"group\":\"test\",\"elapsed_ns\":1,\"total_bytes\":1,\"total_documents\":1,\"mismatch_count\":0,\"mib_per_sec\":100.0,\"docs_per_sec\":100.0,\"ns_per_byte\":1.0},"
  "{\"name\":\"bench/b\",\"group\":\"test\",\"elapsed_ns\":1,\"total_bytes\":1,\"total_documents\":1,\"mismatch_count\":0,\"mib_per_sec\":100.0,\"docs_per_sec\":100.0,\"ns_per_byte\":1.0},"
  "{\"name\":\"bench/c\",\"group\":\"test\",\"elapsed_ns\":1,\"total_bytes\":1,\"total_documents\":1,\"mismatch_count\":0,\"mib_per_sec\":106.0,\"docs_per_sec\":106.0,\"ns_per_byte\":1.0},"
  "{\"name\":\"bench/d\",\"group\":\"test\",\"elapsed_ns\":1,\"total_bytes\":1,\"total_documents\":1,\"mismatch_count\":0,\"mib_per_sec\":112.0,\"docs_per_sec\":112.0,\"ns_per_byte\":1.0}"
  "${_common_suffix}")

file(WRITE "${_latest_broken}"
  "${_common_prefix}"
  "{\"name\":\"bench/a\",\"group\":\"test\",\"elapsed_ns\":1,\"total_bytes\":1,\"total_documents\":1,\"mismatch_count\":1,\"mib_per_sec\":0.0,\"docs_per_sec\":0.0,\"ns_per_byte\":0.0},"
  "{\"name\":\"bench/b\",\"group\":\"test\",\"elapsed_ns\":1,\"total_bytes\":1,\"total_documents\":1,\"mismatch_count\":0,\"mib_per_sec\":100.0,\"docs_per_sec\":100.0,\"ns_per_byte\":1.0},"
  "{\"name\":\"bench/c\",\"group\":\"test\",\"elapsed_ns\":1,\"total_bytes\":1,\"total_documents\":1,\"mismatch_count\":0,\"mib_per_sec\":100.0,\"docs_per_sec\":100.0,\"ns_per_byte\":1.0},"
  "{\"name\":\"bench/d\",\"group\":\"test\",\"elapsed_ns\":1,\"total_bytes\":1,\"total_documents\":1,\"mismatch_count\":0,\"mib_per_sec\":100.0,\"docs_per_sec\":100.0,\"ns_per_byte\":1.0}"
  "${_common_suffix}")

execute_process(
  COMMAND "${LONEJSON_BENCH_EXE}" compare "${_baseline}" "${_latest_mixed}"
  RESULT_VARIABLE _compare_status
  OUTPUT_VARIABLE _compare_out
  ERROR_VARIABLE _compare_err)
if(NOT _compare_status EQUAL 0)
  message(FATAL_ERROR "bench compare failed unexpectedly:\n${_compare_out}\n${_compare_err}")
endif()
foreach(_needle "small-reg" "material-reg" "material-imp" "review-imp")
  if(NOT _compare_out MATCHES "${_needle}")
    message(FATAL_ERROR "bench compare output missing ${_needle}:\n${_compare_out}")
  endif()
endforeach()

execute_process(
  COMMAND "${LONEJSON_BENCH_EXE}" gate "${_baseline}" "${_latest_mixed}"
  RESULT_VARIABLE _gate_status
  OUTPUT_VARIABLE _gate_out
  ERROR_VARIABLE _gate_err)
if(_gate_status EQUAL 0)
  message(FATAL_ERROR "bench gate unexpectedly passed on mixed regressions:\n${_gate_out}\n${_gate_err}")
endif()
foreach(_needle
    "small regressions: 1"
    "material regressions: 1"
    "large improvements to review: 1")
  if(NOT _gate_out MATCHES "${_needle}")
    message(FATAL_ERROR "bench gate output missing ${_needle}:\n${_gate_out}\n${_gate_err}")
  endif()
endforeach()

execute_process(
  COMMAND "${LONEJSON_BENCH_EXE}" gate "${_baseline}" "${_latest_improve}"
  RESULT_VARIABLE _improve_status
  OUTPUT_VARIABLE _improve_out
  ERROR_VARIABLE _improve_err)
if(NOT _improve_status EQUAL 0)
  message(FATAL_ERROR "bench gate unexpectedly failed on pure improvements:\n${_improve_out}\n${_improve_err}")
endif()
if(NOT _improve_out MATCHES "large improvements to review: 1")
  message(FATAL_ERROR "bench gate output missing improvement review count:\n${_improve_out}")
endif()

execute_process(
  COMMAND "${LONEJSON_BENCH_EXE}" compare "${_baseline}" "${_latest_broken}"
  RESULT_VARIABLE _broken_compare_status
  OUTPUT_VARIABLE _broken_compare_out
  ERROR_VARIABLE _broken_compare_err)
if(NOT _broken_compare_status EQUAL 0)
  message(FATAL_ERROR "bench compare failed unexpectedly on broken data:\n${_broken_compare_out}\n${_broken_compare_err}")
endif()
if(NOT _broken_compare_out MATCHES "broken")
  message(FATAL_ERROR "bench compare did not classify mismatching results as broken:\n${_broken_compare_out}")
endif()

execute_process(
  COMMAND "${LONEJSON_BENCH_EXE}" gate "${_baseline}" "${_latest_broken}"
  RESULT_VARIABLE _broken_gate_status
  OUTPUT_VARIABLE _broken_gate_out
  ERROR_VARIABLE _broken_gate_err)
if(_broken_gate_status EQUAL 0)
  message(FATAL_ERROR "bench gate unexpectedly passed on mismatching results:\n${_broken_gate_out}\n${_broken_gate_err}")
endif()
foreach(_needle
    "result mismatches: 1"
    "benchmark gate failed: benchmark case mismatches must be zero before landing")
  if(NOT "${_broken_gate_out}${_broken_gate_err}" MATCHES "${_needle}")
    message(FATAL_ERROR "bench gate output missing ${_needle}:\n${_broken_gate_out}\n${_broken_gate_err}")
  endif()
endforeach()
