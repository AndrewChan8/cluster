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

echo "Conflict-aware adaptive consistency demo"
echo "Leader: $LEADER"

run "$CLIENT" "$LEADER" "$PORT" adaptive on

run "$CLIENT" "$LEADER" "$PORT" append "acctA:100"
run "$CLIENT" "$LEADER" "$PORT" append "acctA:200"
run "$CLIENT" "$LEADER" "$PORT" append "acctA:300"

echo
echo "Check leader logs for:"
echo "CONFLICT DETECTED"
echo "EVENTUAL -> QUORUM"
echo "QUORUM -> STRONG"