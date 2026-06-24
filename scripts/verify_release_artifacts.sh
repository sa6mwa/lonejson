#!/usr/bin/env bash
set -euo pipefail

repo_root=${1:?usage: verify_release_artifacts.sh REPO_ROOT [CHECKSUMS]}
checksums=${2:-}

repo_root="$(CDPATH= cd -- "$repo_root" && pwd)"
if [[ -z "$checksums" ]]; then
  version="$("$repo_root/scripts/release_version.sh")"
  checksums="$repo_root/dist/lonejson-$version-CHECKSUMS"
fi
if [[ ! -f "$checksums" ]]; then
  printf 'missing release checksum manifest: %s\n' "$checksums" >&2
  exit 1
fi

dist_dir="$(CDPATH= cd -- "$(dirname -- "$checksums")" && pwd)"
checksums_name="$(basename -- "$checksums")"
tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

require_command() {
  if ! command -v "$1" >/dev/null 2>&1; then
    printf 'missing required command: %s\n' "$1" >&2
    exit 1
  fi
}

fail_artifact() {
  local artifact=$1
  local path=$2
  local reason=$3
  printf 'release artifact verification failed: %s: %s: %s\n' \
    "$artifact" "$path" "$reason" >&2
  exit 1
}

require_command sha256sum
require_command tar
require_command gzip
require_command grep
require_command strings
require_command file

(cd "$dist_dir" && sha256sum -c "$checksums_name" >/dev/null)

declare -a artifacts=()
while read -r _hash artifact; do
  [[ -n "${artifact:-}" ]] || continue
  case "$artifact" in
    /* | *".."* | *"/.."* | *"../"*)
      printf 'unsafe artifact path in checksum manifest: %s\n' "$artifact" >&2
      exit 1
      ;;
  esac
  if [[ ! -f "$dist_dir/$artifact" ]]; then
    printf 'checksum-listed artifact missing under dist: %s\n' "$artifact" >&2
    exit 1
  fi
  artifacts+=("$artifact")
done <"$checksums"

if [[ ${#artifacts[@]} -eq 0 ]]; then
  printf 'checksum manifest lists no release artifacts: %s\n' "$checksums" >&2
  exit 1
fi

while IFS= read -r dist_file; do
  file_name="$(basename -- "$dist_file")"
  listed=0
  for artifact in "${artifacts[@]}"; do
    if [[ "$artifact" == "$file_name" ]]; then
      listed=1
      break
    fi
  done
  if [[ "$listed" -ne 1 ]]; then
    printf 'release-looking artifact under dist is not checksum-listed: %s\n' \
      "$file_name" >&2
    exit 1
  fi
done < <(find "$dist_dir" -maxdepth 1 -type f \
  \( -name '*.tar.gz' -o -name '*.tgz' -o -name '*.zip' -o -name '*.rock' \
     -o -name '*.src.rock' -o -name '*.rockspec' -o -name '*.h.gz' \) |
  sort)

scan_text_for_local_paths() {
  local artifact=$1
  local path=$2
  local file_path=$3
  local home_path=${HOME:-}

  if grep -aF "$repo_root" "$file_path" >/dev/null 2>&1; then
    fail_artifact "$artifact" "$path" "repository path leaked"
  fi
  if [[ -n "$home_path" ]] && grep -aF "$home_path" "$file_path" >/dev/null 2>&1; then
    fail_artifact "$artifact" "$path" "home path leaked"
  fi
  if grep -aE 'file:///(home|tmp|var/tmp|private/tmp)/|/\.deps/|/build/' \
      "$file_path" >/dev/null 2>&1; then
    fail_artifact "$artifact" "$path" "local file URL or build/cache path leaked"
  fi
}

scan_payload_for_local_paths() {
  local artifact=$1
  local root=$2

  while IFS= read -r file_path; do
    rel_path="${file_path#"$root"/}"
    scan_text_for_local_paths "$artifact" "$rel_path" "$file_path"
  done < <(find "$root" -type f | sort)
}

scan_shipped_payload_for_instrumentation() {
  local artifact=$1
  local root=$2

  while IFS= read -r file_path; do
    rel_path="${file_path#"$root"/}"
    case "$rel_path" in
      *.c | *.h | *.lua | *.md | *.txt | */RELEASE_MANIFEST | */VERSION)
        continue
        ;;
    esac
    case "$rel_path" in
      *liblonejson*.a | *liblonejson*.so* | *liblonejson*.dylib | \
      *.rockspec | *.h | *.pc | *.cmake | *.so | *.dylib)
        if strings "$file_path" | grep -E \
          '(__asan|__msan|__tsan|libasan|libmsan|libtsan|AddressSanitizer|MemorySanitizer|ThreadSanitizer|LLVMFuzzerTestOneInput|libFuzzer|-fsanitize)' \
          >/dev/null; then
          fail_artifact "$artifact" "$rel_path" \
            "sanitizer or fuzzer instrumentation marker leaked"
        fi
        ;;
    esac
  done < <(find "$root" -type f | sort)
}

scan_loader_metadata() {
  local artifact=$1
  local root=$2

  while IFS= read -r file_path; do
    rel_path="${file_path#"$root"/}"
    description="$(file -b "$file_path")"
    case "$description" in
      *ELF*shared\ object* | *ELF*executable*)
        if command -v readelf >/dev/null 2>&1; then
          metadata="$(readelf -d "$file_path" 2>/dev/null || true)"
          if printf '%s\n' "$metadata" | grep -E \
              '(libasan|libmsan|libtsan|RUNPATH|RPATH).*(/home/|/tmp/|/var/tmp|/build/|/\.deps/)' \
              >/dev/null; then
            fail_artifact "$artifact" "$rel_path" \
              "non-relocatable or sanitizer ELF loader metadata"
          fi
        fi
        ;;
      *Mach-O* | *Mach-O\ 64-bit*)
        if command -v otool >/dev/null 2>&1; then
          metadata="$(otool -L "$file_path" 2>/dev/null || true; otool -l "$file_path" 2>/dev/null || true)"
          if printf '%s\n' "$metadata" | grep -E \
              '(libasan|libmsan|libtsan|/home/|/tmp/|/var/tmp|/build/|/\.deps/)' \
              >/dev/null; then
            fail_artifact "$artifact" "$rel_path" \
              "non-relocatable or sanitizer Mach-O loader metadata"
          fi
        fi
        ;;
    esac
  done < <(find "$root" -type f | sort)
}

extract_artifact() {
  local artifact=$1
  local source_path="$dist_dir/$artifact"
  local artifact_root="$tmp_dir/extract/$artifact"

  mkdir -p "$artifact_root"
  case "$artifact" in
    *.tar.gz | *.tgz)
      tar -xzf "$source_path" -C "$artifact_root"
      ;;
    *.src.rock | *.rock | *.zip)
      require_command unzip
      unzip -q "$source_path" -d "$artifact_root"
      ;;
    *.h.gz)
      gzip -cd "$source_path" >"$artifact_root/${artifact%.gz}"
      ;;
    *)
      cp "$source_path" "$artifact_root/"
      ;;
  esac

  while IFS= read -r nested; do
    nested_root="$nested.expanded"
    mkdir -p "$nested_root"
    case "$nested" in
      *.tar.gz | *.tgz)
        tar -xzf "$nested" -C "$nested_root"
        ;;
      *.src.rock | *.rock | *.zip)
        require_command unzip
        unzip -q "$nested" -d "$nested_root"
        ;;
      *.gz)
        gzip -cd "$nested" >"$nested_root/$(basename "${nested%.gz}")"
        ;;
    esac
  done < <(find "$artifact_root" -type f \
    \( -name '*.tar.gz' -o -name '*.tgz' -o -name '*.zip' -o -name '*.rock' \
       -o -name '*.src.rock' -o -name '*.h.gz' \) |
    sort)

  scan_payload_for_local_paths "$artifact" "$artifact_root"
  scan_shipped_payload_for_instrumentation "$artifact" "$artifact_root"
  scan_loader_metadata "$artifact" "$artifact_root"
}

for artifact in "${artifacts[@]}"; do
  scan_text_for_local_paths "$checksums_name" "$artifact" "$dist_dir/$artifact"
  extract_artifact "$artifact"
done

printf 'release artifact verification passed: %s\n' "$checksums"
