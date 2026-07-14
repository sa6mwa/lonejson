#!/usr/bin/env bash
set -euo pipefail

# Keeping services retains the stack after a successful e2e run; it must not
# skip the startup that makes a clean checkout ready for the focused tests.

repo_root=$1
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

test_root="$tmp_dir/repo"
test_scripts="$test_root/scripts"
fake_bin="$tmp_dir/bin"
event_log="$tmp_dir/events"
mkdir -p "$test_scripts" "$fake_bin"
cp "$repo_root/scripts/test-e2e.sh" "$test_scripts/test-e2e.sh"

cat >"$test_scripts/dev-up.sh" <<'EOF'
#!/usr/bin/env bash
printf '%s\n' up >>"${LONEJSON_E2E_TEST_EVENT_LOG:?}"
EOF
cat >"$test_scripts/dev-down.sh" <<'EOF'
#!/usr/bin/env bash
printf '%s\n' down >>"${LONEJSON_E2E_TEST_EVENT_LOG:?}"
EOF
cat >"$test_scripts/dev-ps.sh" <<'EOF'
#!/usr/bin/env bash
exit 0
EOF
cat >"$test_scripts/compose.sh" <<'EOF'
#!/usr/bin/env bash
exit 0
EOF
cat >"$test_scripts/time_step.sh" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
printf 'step:%s\n' "$1" >>"${LONEJSON_E2E_TEST_EVENT_LOG:?}"
shift
exec "$@"
EOF
cat >"$fake_bin/make" <<'EOF'
#!/usr/bin/env bash
printf 'make:%s\n' "$*" >>"${LONEJSON_E2E_TEST_EVENT_LOG:?}"
EOF
chmod +x "$test_scripts"/*.sh "$fake_bin/make"

PATH="$fake_bin:$PATH" \
  LONEJSON_E2E_TEST_EVENT_LOG="$event_log" \
  LONEJSON_E2E_KEEP_DEVSERVICES=1 \
  "$test_scripts/test-e2e.sh"

[[ "$(grep -cx up "$event_log")" -eq 1 ]]
[[ "$(head -n 1 "$event_log")" == up ]]
if grep -Fx down "$event_log" >/dev/null; then
  printf 'e2e keep-services mode tore down the retained stack\n' >&2
  exit 1
fi
for step in e2e/curl e2e/oidc e2e/m2m; do
  grep -Fx "step:$step" "$event_log" >/dev/null
done
