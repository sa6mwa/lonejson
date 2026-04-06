#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

repeat_block() {
  local block="$1"
  local count="$2"
  local i
  for ((i = 0; i < count; i += 1)); do
    printf '%s' "$block"
  done
}

write_mapped_large() {
  local out="$ROOT_DIR/fuzz/corpus/mapped/person_large_payload.json"
  : >"$out"
  {
    printf '{'
    printf '"name":"'
    repeat_block 'MappedLargeName_' 4096
    printf '",'
    printf '"nickname":"'
    repeat_block 'nick' 1024
    printf '",'
    printf '"age":42,'
    printf '"active":true,'
    printf '"address":{"city":"'
    repeat_block 'NordicCity_' 1024
    printf '","zip":12345},'
    printf '"lucky_numbers":['
    local i
    for ((i = 0; i < 4096; i += 1)); do
      if ((i != 0)); then
        printf ','
      fi
      printf '%d' "$((i * 7))"
    done
    printf '],'
    printf '"tags":['
    for ((i = 0; i < 512; i += 1)); do
      if ((i != 0)); then
        printf ','
      fi
      printf '"tag_%04d_' "$i"
      repeat_block 'x' 16
      printf '"'
    done
    printf ']'
    printf '}'
  } >>"$out"
}

write_json_value_large() {
  local out="$ROOT_DIR/fuzz/corpus/json_value/large_selector_payload.json"
  : >"$out"
  {
    printf '{'
    printf '"id":"large-seed",'
    printf '"selector":{"where":{"and":['
    local i
    for ((i = 0; i < 256; i += 1)); do
      if ((i != 0)); then
        printf ','
      fi
      printf '{"field":"field_%03d","op":"contains","value":"' "$i"
      repeat_block 'selector_payload_' 64
      printf '"}'
    done
    printf ']}},'
    printf '"fields":{"include":['
    for ((i = 0; i < 2048; i += 1)); do
      if ((i != 0)); then
        printf ','
      fi
      printf '"field_%04d"' "$i"
    done
    printf '],"metadata":{"note":"'
    repeat_block 'metadata_block_' 2048
    printf '"}},'
    printf '"last_error":{"message":"'
    repeat_block 'error_context_' 1024
    printf '","retryable":false,"code":500}'
    printf '}'
  } >>"$out"
}

write_value_visitor_large() {
  local out="$ROOT_DIR/fuzz/corpus/value_visitor/large_unicode_payload.json"
  : >"$out"
  {
    printf '{"meta":{"title":"'
    repeat_block '訪問者_' 2048
    printf '","description":"'
    repeat_block 'مرحبا_' 2048
    printf '"},"items":['
    local i
    for ((i = 0; i < 1024; i += 1)); do
      if ((i != 0)); then
        printf ','
      fi
      printf '{"id":%d,"name":"項目_%04d_' "$i" "$i"
      repeat_block 'שלום_' 8
      printf '","flags":[true,false,null],"score":%d,"payload":"' "$((i * 13))"
      repeat_block 'chunk_' 32
      printf '"}'
    done
    printf ']}'
  } >>"$out"
}

write_mapped_large
write_json_value_large
write_value_visitor_large
