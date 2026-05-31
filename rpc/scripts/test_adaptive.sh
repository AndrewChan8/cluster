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

echo "Adaptive consistency demo"
echo "Leader: $LEADER"

run "$CLIENT" "$LEADER" "$PORT" adaptive on
run "$CLIENT" "$LEADER" "$PORT" drop 100
run "$CLIENT" "$LEADER" "$PORT" append "adaptive escalation test"

run "$CLIENT" "$LEADER" "$PORT" drop 0
run "$CLIENT" "$LEADER" "$PORT" append "adaptive recovery one"
run "$CLIENT" "$LEADER" "$PORT" append "adaptive recovery two"

run "$CLIENT" "$LEADER" "$PORT" adaptive off

echo
echo "Check leader logs for:"
echo "EVENTUAL -> QUORUM"
echo "QUORUM -> STRONG"
echo "STRONG -> QUORUM"
echo "QUORUM -> EVENTUAL"