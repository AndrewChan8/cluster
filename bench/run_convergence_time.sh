#!/usr/bin/env bash
set -euo pipefail

CLIENT="./rpc/client"
PORT="5000"
LEADER="${1:-node2}"
TARGET="${2:-node3}"
OUTDIR="bench/results"
STAMP="$(date +%Y%m%d_%H%M%S)"
OUTFILE="$OUTDIR/convergence_time_$STAMP.csv"

mkdir -p "$OUTDIR"

echo "timestamp,leader,target,result,convergence_ms,leader_status,target_status" > "$OUTFILE"

get_status() {
  "$CLIENT" "$1" "$PORT" status | tail -n 1
}

echo "Running convergence-time experiment"
echo "leader=$LEADER target=$TARGET"

"$CLIENT" "$LEADER" "$PORT" mode quorum >/dev/null
"$CLIENT" "$LEADER" "$PORT" partition "$TARGET" >/dev/null

"$CLIENT" "$LEADER" "$PORT" append "convergence partition test" >/dev/null

"$CLIENT" "$LEADER" "$PORT" heal >/dev/null

start="$(date +%s%3N)"
result="fail"

for _ in $(seq 1 20); do
  leader_status="$(get_status "$LEADER")"
  target_status="$(get_status "$TARGET")"

  if [ "$leader_status" = "$target_status" ]; then
    result="ok"
    break
  fi

  sleep 1
done

end="$(date +%s%3N)"
duration=$((end - start))

echo "$(date -Iseconds),$LEADER,$TARGET,$result,$duration,\"$leader_status\",\"$target_status\"" >> "$OUTFILE"

echo "result=$result convergence_ms=$duration"
echo "leader_status=$leader_status"
echo "target_status=$target_status"
echo
echo "Wrote results to $OUTFILE"