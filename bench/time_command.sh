#!/usr/bin/env bash
set -uo pipefail

if [ "$#" -lt 1 ]; then
  echo "Usage: $0 <command> [args...]"
  exit 1
fi

start_ns=$(date +%s%N)

"$@"
status=$?

end_ns=$(date +%s%N)
elapsed_ns=$((end_ns - start_ns))
elapsed_ms=$((elapsed_ns / 1000000))

echo "elapsed_ms=$elapsed_ms"
echo "exit_status=$status"

exit "$status"