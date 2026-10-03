#!/usr/bin/env bash
set -euo pipefail
# Each fixture supplies its own version state, independent of candidate builds.
unset LONEJSON_VERSION_OVERRIDE
root=$1
mkdir -p "$root/build"
workspace=$(mktemp -d "$root/build/test-version-detection.XXXXXX")
trap 'rm -rf "$workspace"' EXIT
mkdir "$workspace/bin"
cat >"$workspace/bin/git" <<'EOF'
#!/usr/bin/env bash
shift 2
case "$1" in
  rev-parse) printf '%s\n' "$TEST_VERSION_ROOT" ;;
  tag) printf '%s\n' "$TEST_VERSION_TAGS" ;;
  cat-file)
    case "${TEST_VERSION_OBJECT:-}:$3" in
      tag:*|*:refs/tags/v9.8.7) printf 'tag\n' ;;
      *) printf 'commit\n' ;;
    esac
    ;;
  *) exit 1 ;;
esac
EOF
chmod +x "$workspace/bin/git"
detect() {
  PATH="$workspace/bin:$PATH" TEST_VERSION_ROOT="$root" \
    "$root/scripts/release_version.sh"
}
TEST_VERSION_TAGS=v1.2.3 detect | grep -qx 1.2.3
# Annotated and signed releases both have Git object type "tag".
TEST_VERSION_OBJECT=tag TEST_VERSION_TAGS=v1.2.3 detect | grep -qx 0.0.0
TEST_VERSION_OBJECT=tag TEST_VERSION_TAGS=v1.2.3 LONEJSON_VERSION_OVERRIDE=2.3.4 detect | grep -qx 2.3.4
TEST_VERSION_TAGS=$'v9.8.7\nv1.2.3' LONEJSON_VERSION_OVERRIDE=bad detect | grep -qx 1.2.3
TEST_VERSION_TAGS=$'v1.2.3-rc1\nother' detect | grep -qx 0.0.0
if TEST_VERSION_TAGS= LONEJSON_VERSION_OVERRIDE=bad detect >"$workspace/invalid.log" 2>&1; then
  printf 'invalid version override accepted\n' >&2; exit 1
fi
