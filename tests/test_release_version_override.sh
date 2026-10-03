#!/usr/bin/env bash
set -euo pipefail
repo_root=$(CDPATH= cd -- "$1" && pwd -P)
reserved=v99.99.99
owned_tag=0
cleanup() {
  if [[ "$owned_tag" == 1 ]]; then
    git -C "$repo_root" tag -d "$reserved" >/dev/null
  fi
}
trap cleanup EXIT
# Recover only the reserved fixture, before consulting exact tags.
if git -C "$repo_root" show-ref --verify --quiet "refs/tags/$reserved"; then
  git -C "$repo_root" tag -d "$reserved" >/dev/null
fi
exact_tag=
exact_tags="$(git -C "$repo_root" tag --points-at HEAD --sort=-version:refname)"
while IFS= read -r tag; do
  [[ "$tag" =~ ^v[0-9]+[.][0-9]+[.][0-9]+$ ]] || continue
  if [[ "$(git -C "$repo_root" cat-file -t "refs/tags/$tag")" != commit ]]; then
    printf 'lifecycle-version-contract: exact release tag %s must be lightweight\n' "$tag" >&2
    exit 1
  fi
  [[ -n "$exact_tag" ]] || exact_tag=$tag
done <<<"$exact_tags"
check_version() {
  local expected=$1
  env -u LONEJSON_VERSION_OVERRIDE "$repo_root/scripts/release_version.sh" | grep -qx "$expected"
  env -u LONEJSON_VERSION_OVERRIDE make --no-print-directory -s -C "$repo_root" print-release-version LONEJSON_VERSION_OVERRIDE= | grep -qx "$expected"
}
if [[ -n "$exact_tag" ]]; then
  check_version "${exact_tag#v}"
  LONEJSON_VERSION_OVERRIDE=bad "$repo_root/scripts/release_version.sh" | grep -qx "${exact_tag#v}"
  make --no-print-directory -s -C "$repo_root" print-release-version LONEJSON_VERSION_OVERRIDE=bad | grep -qx "${exact_tag#v}"
else
  check_version 0.0.0
  LONEJSON_VERSION_OVERRIDE=7.8.9 "$repo_root/scripts/release_version.sh" | grep -qx 7.8.9
  make --no-print-directory -s -C "$repo_root" print-release-version LONEJSON_VERSION_OVERRIDE=7.8.9 | grep -qx 7.8.9
  # Explicit command configuration must win even when user settings require signing.
  owned_tag=1
  GIT_CONFIG_COUNT=1 GIT_CONFIG_KEY_0=tag.gpgSign GIT_CONFIG_VALUE_0=true \
    git -C "$repo_root" -c tag.gpgSign=false tag "$reserved"
  [[ "$(git -C "$repo_root" cat-file -t "refs/tags/$reserved")" == commit ]]
  check_version "${reserved#v}"
  LONEJSON_VERSION_OVERRIDE=bad "$repo_root/scripts/release_version.sh" | grep -qx "${reserved#v}"
  make --no-print-directory -s -C "$repo_root" print-release-version LONEJSON_VERSION_OVERRIDE=bad | grep -qx "${reserved#v}"
  cleanup
  owned_tag=0
  check_version 0.0.0
fi
printf '%s\n' 'Lightweight-tag Make/script version contract passed.'
