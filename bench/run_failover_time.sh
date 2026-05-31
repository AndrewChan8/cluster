#!/usr/bin/env bash
set -euo pipefail

CLIENT="./rpc/client"
PORT="5000"
OLD_LEADER="${1:-node2}"
OUTDIR="bench/results"
STAMP="$(date +%Y%m%d_%H%M%S)"
OUTFILE="$OUTDIR/failover_time_$STAMP.csv"

mkdir -p "$OUTDIR"

echo "timestamp,old_leader,new_leader,result,failover_ms" > "$OUTFILE"

echo "Failover-time experiment"
echo "Current leader is expected to be: $OLD_LEADER"
echo
echo "Now manually stop $OLD_LEADER, then press ENTER."
read -r

start="$(date +%s%3N)"
result="fail"
new_leader=""

for _ in $(seq 1 30); do
  for node in node1 node2 node3; do
    if [ "$node" = "$OLD_LEADER" ]; then
      continue
    fi

    output="$("$CLIENT" "$node" "$PORT" mode quorum 2>&1 || true)"

    if echo "$output" | grep -q "mode succeeded"; then
      new_leader="$node"
      result="ok"
      break 2
    fi
  done

  sleep 1
done

end="$(date +%s%3N)"
duration=$((end - start))

echo "$(date -Iseconds),$OLD_LEADER,$new_leader,$result,$duration" >> "$OUTFILE"

echo "result=$result old_leader=$OLD_LEADER new_leader=$new_leader failover_ms=$duration"
echo "Wrote results to $OUTFILE"