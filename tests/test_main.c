#include "test_support.inc"
#include "test_protocol_framing.inc"
#include "test_parse_serialize.inc"
#include "test_array_stream.inc"
#include "test_fixtures_public.inc"
#include "test_json_value_sources.inc"
#include "test_allocator_visitor.inc"
#include "test_generator_omit.inc"
#include "test_curl_misc.inc"

int main(void) {
  test_parse_implicit_destination_reset();
  test_dynamic_allocation_cleanup_balance();
  test_dynamic_allocation_reset_reparse_balance();
  test_zero_alloc_validate_path();
  test_parser_workspace_accounting();
  test_parser_workspace_alignment_regression();
  test_init_does_not_preserve_garbage_array_state();
  test_zero_alloc_fixed_storage_parse();
  test_zero_alloc_fixed_storage_serialize();
  test_zero_alloc_jsonl_serialize();
  test_serialize_alloc_balance();
  test_serialize_owned_output_cap_is_enforced();
  test_serialize_owned_output_cap_allows_exact_payload();
  test_serialize_alloc_zero_cap_uses_safe_default();
  test_serialize_alloc_default_free_compatibility();
  test_serialize_jsonl_alloc_balance();
  test_serialize_jsonl_alloc_output_cap_is_enforced();
  test_serialize_alloc_pretty_output_cap_is_enforced();
  test_serialize_jsonl_alloc_default_free_compatibility();
  test_parse_buffer_basic();
  test_parse_fixed_strings_with_escapes_and_truncation();
  test_chunked_parser_and_missing_required();
  test_object_framed_stream_jsonl();
  test_stream_reuses_and_changes_destination();
  test_object_stream_nonblocking_fd_would_block();
  test_array_stream_mapped_items_from_root_key();
  test_array_stream_raw_values_from_root_array();
  test_array_stream_open_path_and_fd_sources();
  test_array_stream_failure_modes();
  test_array_stream_streaming_invariants();
  test_array_stream_delimiter_validation_before_yield();
  test_array_stream_mapped_items_do_not_materialize_item();
  test_array_stream_skips_do_not_materialize_values();
  test_array_stream_escaped_key_path_matching();
  test_array_stream_escaped_key_failure_modes();
  test_array_stream_duplicate_rejection_invariants();
  test_array_stream_raw_duplicate_check_allows_large_values();
  test_array_stream_reader_tolerates_empty_reads();
  test_array_stream_additional_failure_modes();
  test_array_stream_would_block_boundaries();
  test_array_stream_depth_limit_and_argument_guards();
  test_array_stream_push_api();
  test_array_stream_push_invariant_failure_modes();
  test_array_stream_push_does_not_materialize_response();
  test_array_stream_push_cleans_reused_destination();
  test_array_stream_push_string_items();
  test_array_stream_push_string_item_callback();
  test_array_stream_push_string_item_callback_bounds();
  test_array_stream_push_string_failure_modes();
  test_file_and_buffer_helpers();
  test_jsonl_helpers();
  test_sse_incremental_events_and_json_selection();
  test_sse_json_streams_data_without_event_buffering();
  test_sse_json_event_limits_and_boundary_errors();
  test_sse_json_event_limits_alloc_guarded_e2e();
  test_sse_retry_and_json_metadata_only_events();
  test_sse_callback_failure_status();
  test_sse_limits_and_allocation_contract();
  test_sse_limits_and_invalid_usage();
  test_multipart_incremental_parts_and_raw_payloads();
  test_multipart_no_length_preserves_body_bytes();
  test_multipart_no_length_streams_long_lines_with_lookahead();
  test_multipart_content_length_streams_body_without_body_allocations();
  test_multipart_content_disposition_name_parameters();
  test_multipart_limits_and_invalid_usage();
  test_multipart_failures_are_reported();
  test_fixed_capacity_object_array();
  test_dynamic_object_array_alignment();
  test_formatting_variants_and_roundtrip();
  test_duplicate_key_policy();
  test_public_api_argument_and_serialization_guards();
  test_public_initializers_and_defaults();
  test_small_valid_spec_fixtures();
  test_invalid_json_vectors();
  test_fixture_documents();
  test_large_generated_fixture_sizes();
  test_large_generated_fixture_parsing();
  test_json_test_suite_parsing();
  test_spooled_fields_roundtrip();
  test_spooled_fields_small_and_null();
  test_spooled_append_api();
  test_spooled_field_failures();
  test_source_fields_path_and_raw_sink();
  test_source_fields_file_and_fd();
  test_source_field_parse_and_seek_failures();
  test_source_fields_do_not_mutate_sink_on_open_failure();
  test_json_value_parse_and_roundtrip();
  test_json_value_parse_capture_escapes();
  test_json_value_parse_stream_sink();
  test_json_value_parse_visitor();
  test_json_value_parse_visitor_fast_path_regressions();
  test_json_value_setters_and_failures();
  test_json_value_scalars_null_and_reset();
  test_json_value_reuse_and_cleanup_ownership();
  test_json_value_source_validation_failures();
  test_json_value_nonseekable_and_sink_failures();
  test_json_value_large_source_backed_serialization();
  test_json_value_parse_requires_destination_and_partial_failure();
  test_json_value_nested_failure_matrix();
  test_value_visitor_success_and_limits();
  test_value_visitor_chunking_unicode_and_failures();
  test_custom_allocator_parse_cleanup_and_stream();
#ifndef NDEBUG
  test_custom_allocator_misaligned_owned_alloc_is_rejected();
#endif
  test_custom_allocator_json_value_capture_and_serialize_alloc();
  test_custom_allocator_raw_serialize_alloc_is_rejected();
  test_explicit_default_allocator_raw_serialize_alloc_is_allowed();
  test_explicit_default_allocator_raw_serialize_jsonl_alloc_is_allowed();
  test_custom_allocator_json_value_capture_preserves_handle_allocator();
  test_partial_allocator_parse_is_rejected();
  test_partial_allocator_parse_reader_is_rejected();
  test_partial_allocator_serialize_alloc_is_rejected();
  test_partial_allocator_generator_init_is_rejected();
  test_generator_compact_streams_event_map();
  test_generator_pretty_streams_source_field();
  test_generator_streams_json_value_fields();
  test_generator_pretty_streams_json_value_reader();
  test_serialize_omits_empty_optional_fields();
  test_serialize_omit_layout_failures();
  test_serialize_omit_pretty_and_recursive_invariants();
  test_serialize_omit_empty_implies_null();
  test_rewindability_edges_and_measure_preserves_sources();
#ifdef LONEJSON_WITH_CURL
  test_curl_upload_cleanup_default_allocator();
  test_curl_upload_custom_allocator_balance();
  test_curl_upload_streaming_does_not_buffer_payload();
  test_curl_array_parse_streams_selected_arrays();
  test_curl_array_parse_failure_cleanup_and_truncation();
  test_curl_string_array_parse_streams_keys();
  test_curl_string_items_parse_streams_keys();
#endif
  test_clear_destination_ignores_poisoned_stream_and_json_value_state();
  test_prepared_fixed_object_array_parse_preserves_storage();

  if (g_failures != 0) {
    fprintf(stderr, "test failures: %d\n", g_failures);
    return 1;
  }
  return 0;
}
