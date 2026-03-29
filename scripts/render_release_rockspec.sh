#!/usr/bin/env bash
set -eu

if [ "$#" -lt 2 ] || [ "$#" -gt 4 ]; then
  printf '%s\n' "usage: $0 VERSION OUTPUT_PATH [SOURCE_URL] [SOURCE_TAG]" >&2
  exit 1
fi

version="$1"
output_path="$2"
source_url="git+https://github.com/sa6mwa/lonejson.git"
source_tag=""
tag_line=""

if [ "$#" -ge 3 ]; then
  source_url="$3"
fi

if [ "$#" -ge 4 ]; then
  source_tag="$4"
elif [ "$version" != "0.0.0" ]; then
  source_tag="v$version"
fi

if [ -n "$source_tag" ]; then
  tag_line='  tag = "'"$source_tag"'",'
fi

sed \
  -e "s|@VERSION@|$version|g" \
  -e "s|@SOURCE_URL@|$source_url|g" \
  -e "s|@SOURCE_TAG_LINE@|$tag_line|g" \
  lonejson.rockspec.in >"$output_path"
