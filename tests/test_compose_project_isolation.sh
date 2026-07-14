#!/usr/bin/env bash
set -euo pipefail

repo_root=$1
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

fake_bin="$tmp_dir/bin"
event_log="$tmp_dir/events"
mkdir -p "$fake_bin" "$tmp_dir/first/scripts" "$tmp_dir/second/scripts"
for checkout in first second; do
  cp "$repo_root/scripts/compose.sh" "$tmp_dir/$checkout/scripts/compose.sh"
  cp "$repo_root/docker-compose.yaml" "$tmp_dir/$checkout/docker-compose.yaml"
done

cat >"$fake_bin/docker" <<'EOF'
#!/usr/bin/env bash
printf '%s\n' "${LONEJSON_COMPOSE_PROJECT_NAME:?}" >>"${LONEJSON_COMPOSE_TEST_EVENT_LOG:?}"
EOF
cp "$fake_bin/docker" "$fake_bin/nerdctl"
chmod +x "$fake_bin/docker" "$fake_bin/nerdctl" "$tmp_dir"/*/scripts/compose.sh

PATH="$fake_bin:$PATH" \
  LONEJSON_COMPOSE_TEST_EVENT_LOG="$event_log" \
  "$tmp_dir/first/scripts/compose.sh" ps
PATH="$fake_bin:$PATH" \
  LONEJSON_COMPOSE_TEST_EVENT_LOG="$event_log" \
  "$tmp_dir/second/scripts/compose.sh" ps
PATH="$fake_bin:$PATH" \
  LONEJSON_COMPOSE_TEST_EVENT_LOG="$event_log" \
  LONEJSON_COMPOSE_PROJECT_NAME=chosen-project \
  "$tmp_dir/first/scripts/compose.sh" ps

mapfile -t project_names <"$event_log"
[[ "${#project_names[@]}" -eq 3 ]]
[[ "${project_names[0]}" == lonejson-e2e-* ]]
[[ "${project_names[1]}" == lonejson-e2e-* ]]
[[ "${project_names[0]}" != "${project_names[1]}" ]]
[[ "${project_names[2]}" == chosen-project ]]
