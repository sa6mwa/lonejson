#!/usr/bin/env bash
# Native Apple extension to lifecycle-owned Linux/osxcross resolution.
print_darwin_target() {
  local sdk tool path
  [[ "$("${UNAME:-uname}" -m)" == arm64 ]] || die 'native Darwin builds require Apple Silicon (arm64)'
  sdk=$(xcrun --sdk macosx --show-sdk-path) || die 'macOS SDK unavailable; install Xcode or Command Line Tools'
  [[ -d "$sdk" ]] || die 'xcrun returned an unavailable macOS SDK'
  printf 'target=arm64-apple-darwin\nsource=apple\ndownloadable=no\nsysroot=%s\n' "$sdk"
  for tool in clang clang++ ld ar ranlib strip nm otool install_name_tool; do
    path=$(xcrun --sdk macosx --find "$tool") || die "Apple tool unavailable: $tool"
    [[ -x "$path" ]] || die "Apple tool is not executable: $tool"
    case "$tool" in
      clang) printf 'root=%s\ncc=%s\n' "${path%/bin/*}" "$path" ;;
      clang++) printf 'cxx=%s\n' "$path" ;;
      *) printf '%s=%s\n' "$tool" "$path" ;;
    esac
  done
  printf 'status=ready\n'
}
