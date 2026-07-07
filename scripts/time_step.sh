#!/usr/bin/env bash

set -u

if [ "$#" -lt 2 ]; then
  printf 'usage: %s STEP COMMAND [ARG...]\n' "$0" >&2
  exit 2
fi

step=$1
shift

start_epoch=$(date +%s)
start_iso=$(date -u '+%Y-%m-%dT%H:%M:%SZ')
printf 'PKT_TIMING_BEGIN step=%s started_at=%s\n' "$step" "$start_iso"

"$@"
status=$?

end_epoch=$(date +%s)
end_iso=$(date -u '+%Y-%m-%dT%H:%M:%SZ')
duration=$((end_epoch - start_epoch))
printf 'PKT_TIMING_END step=%s status=%s seconds=%s finished_at=%s\n' \
  "$step" "$status" "$duration" "$end_iso"

exit "$status"
