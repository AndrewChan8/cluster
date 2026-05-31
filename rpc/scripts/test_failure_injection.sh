#!/usr/bin/env bash
set -euo pipefail

CLIENT="./client"
PORT="5000"
LEADER="${1:-node2}"

run() {
  echo
  echo "=== $* ==="
  "$@"
}

echo "Failure injection test"
echo "Assumes servers are running and $LEADER is leader."

run "$CLIENT" "$LEADER" "$PORT" latency 500
run "$CLIENT" "$LEADER" "$PORT" append "latency injection test"

run "$CLIENT" "$LEADER" "$PORT" latency 0

run "$CLIENT" "$LEADER" "$PORT" drop 100
run "$CLIENT" "$LEADER" "$PORT" mode quorum
run "$CLIENT" "$LEADER" "$PORT" append "drop should fail"

run "$CLIENT" "$LEADER" "$PORT" drop 0
run "$CLIENT" "$LEADER" "$PORT" append "drop disabled test"

run "$CLIENT" "$LEADER" "$PORT" partition node3
run "$CLIENT" "$LEADER" "$PORT" append "partition node3 test"

run "$CLIENT" "$LEADER" "$PORT" heal
run "$CLIENT" "$LEADER" "$PORT" append "heal partition test"

echo
echo "Waiting for anti-entropy repair..."
sleep 7

run "$CLIENT" node1 "$PORT" log
run "$CLIENT" node2 "$PORT" log
run "$CLIENT" node3 "$PORT" log