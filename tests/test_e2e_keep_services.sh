#!/usr/bin/env bash
set -euo pipefail

repo_root=$1
mkdir -p "$repo_root/build"
tmp_dir=$(mktemp -d "$repo_root/build/e2e-cleanup.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT

test_root="$tmp_dir/repo"
test_scripts="$test_root/scripts"
fake_bin="$tmp_dir/bin"
event_log="$tmp_dir/events"
mkdir -p "$test_scripts" "$test_root/tests" "$fake_bin"
cp "$repo_root/scripts/test-e2e.sh" "$test_scripts/test-e2e.sh"
cat >"$test_scripts/devenv.sh" <<'SH'
#!/usr/bin/env bash
printf '%s\n' "$*" >>"$LONEJSON_E2E_TEST_EVENT_LOG"
[[ "$1" != up || "${E2E_TEST_UP_FAIL:-0}" != 1 ]]
SH
cat >"$test_root/tests/test_devenv_ownership.sh" <<'SH'
#!/usr/bin/env bash
printf '%s\n' ownership >>"$LONEJSON_E2E_TEST_EVENT_LOG"
SH
cat >"$test_scripts/time_step.sh" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
printf 'step:%s\n' "$1" >>"$LONEJSON_E2E_TEST_EVENT_LOG"
shift
exec "$@"
SH
cat >"$fake_bin/make" <<'SH'
#!/usr/bin/env bash
printf 'make:%s\n' "$*" >>"$LONEJSON_E2E_TEST_EVENT_LOG"
[[ "${E2E_TEST_MAKE_FAIL:-0}" != 1 ]]
SH
chmod +x "$test_scripts"/*.sh "$test_root/tests/"*.sh "$fake_bin/make"
export PATH="$fake_bin:$PATH" LONEJSON_E2E_TEST_EVENT_LOG="$event_log"
for keep in 0 1; do
  : >"$event_log"
  LONEJSON_E2E_KEEP_DEVSERVICES="$keep" "$test_scripts/test-e2e.sh"
  [[ "$(head -n 1 "$event_log")" == up ]]
  [[ "$(grep -cx up "$event_log")" == 1 ]]
  grep -Fx ownership "$event_log" >/dev/null
  for step in e2e/curl e2e/oidc e2e/m2m; do
    grep -Fx "step:$step" "$event_log" >/dev/null
  done
  if [[ "$keep" == 0 ]]; then
    [[ "$(tail -n 1 "$event_log")" == reset ]]
  else
    ! grep -Ex 'down|reset' "$event_log"
  fi
  for failure in E2E_TEST_UP_FAIL E2E_TEST_MAKE_FAIL; do
    : >"$event_log"
    if env "$failure=1" LONEJSON_E2E_KEEP_DEVSERVICES="$keep" \
        "$test_scripts/test-e2e.sh" >"$tmp_dir/failure" 2>&1; then
      printf 'e2e swallowed a startup or test failure\n' >&2; exit 1
    fi
    grep -Fx ps "$event_log" >/dev/null
    grep -Fx 'logs --tail 120' "$event_log" >/dev/null
    if [[ "$keep" == 0 && "$failure" == E2E_TEST_MAKE_FAIL ]]; then
      [[ "$(tail -n 1 "$event_log")" == down ]]
    else
      ! grep -Ex 'down|reset' "$event_log"
    fi
  done
done
