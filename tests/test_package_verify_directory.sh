#!/usr/bin/env bash
set -euo pipefail
repo_root=${1:?repository root required}
mkdir -p "$repo_root/build"
work=$(mktemp -d "$repo_root/build/package-verify-directory.XXXXXX")
trap 'rm -rf -- "$work"' EXIT
fixture="$work/repo"
custom="$work/custom artifacts"
mkdir -p "$fixture/scripts" "$fixture/cmake" "$custom"
cp "$repo_root/scripts/"{package-verify,require_build_workspace}.sh "$fixture/scripts/"
cp "$repo_root/cmake/"{package_checksums,release_artifacts,lonejson_dist_dir,lonejson_build_workspace}.cmake "$fixture/cmake/"
printf '#!/usr/bin/env bash\nprintf "1.2.3\\n"\n' >"$fixture/scripts/release_version.sh"
cat >"$fixture/scripts/verify_release_artifacts.sh" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
[[ $# == 4 && $3 == "$EXPECTED_PAYLOAD" && $4 == "$EXPECTED_SCOPE" ]]
printf 'privacy\n' >>"$VERIFY_EVENTS"
SH
cat >"$fixture/scripts/verify_release_archives.sh" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
[[ $# == 4 && $4 == "$EXPECTED_PAYLOAD" ]]
printf 'consumers\n' >>"$VERIFY_EVENTS"
SH
chmod +x "$fixture/scripts/"*.sh
for artifact in lonejson-1.2.3.tar.gz lonejson-1.2.3.h.gz lonejson-lua-1.2.3.tar.gz \
    lonejson-1.2.3-1.rockspec lonejson-1.2.3-1.src.rock liblonejson-1.2.3-x86_64-linux-gnu.tar.gz; do
  printf '%s\n' "$artifact" >"$custom/$artifact"
done
printf 'lonejson lifecycle artifact directory\n' >"$custom/.lonejson-dist"
export LONEJSON_RELEASE_TARGETS=x86_64-linux-gnu
cmake -D "LONEJSON_ROOT=$fixture" -D LONEJSON_VERSION=1.2.3 \
  -D "LONEJSON_DIST_DIR=$custom" -D LONEJSON_RELEASE_TARGETS=x86_64-linux-gnu \
  -P "$fixture/cmake/package_checksums.cmake"
export EXPECTED_PAYLOAD="$custom" EXPECTED_SCOPE=full VERIFY_EVENTS="$work/events"
(cd "$work" && "$fixture/scripts/package-verify.sh" 'custom artifacts/lonejson-1.2.3-CHECKSUMS')
printf 'privacy\nconsumers\n' >"$work/expected"
cmp "$work/expected" "$VERIFY_EVENTS"
[[ ! -e "$fixture/dist" ]]
: >"$VERIFY_EVENTS"
printf changed >>"$custom/lonejson-1.2.3.h.gz"
if "$fixture/scripts/package-verify.sh" "$custom/lonejson-1.2.3-CHECKSUMS" >"$work/changed" 2>&1; then
  printf 'package verification accepted changed artifact bytes\n' >&2; exit 1
fi
grep -F 'checksums differ' "$work/changed" >/dev/null
[[ ! -s "$VERIFY_EVENTS" ]]
# Binary manifests live under build/, separately from the dist/ payload.
mkdir -p "$fixture/dist"
cp "$custom/"lonejson-* "$custom/"liblonejson-* "$fixture/dist/"
cmake -D "LONEJSON_ROOT=$fixture" -D LONEJSON_VERSION=1.2.3 \
  -D LONEJSON_RELEASE_TARGETS=x86_64-linux-gnu -D LONEJSON_CHECKSUM_SCOPE=binary \
  -P "$fixture/cmake/package_checksums.cmake"
export EXPECTED_PAYLOAD="$fixture/dist" EXPECTED_SCOPE=binary
"$fixture/scripts/package-verify.sh" --binary
cmp "$work/expected" "$VERIFY_EVENTS"
printf 'Custom full-scope and separate binary-manifest directories passed\n'
