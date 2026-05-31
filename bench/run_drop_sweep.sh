#!/usr/bin/env bash
set -euo pipefail

CLIENT="./rpc/client"
PORT="5000"
LEADER="${1:-node2}"
OUTDIR="bench/results"
STAMP="$(date +%Y%m%d_%H%M%S)"
OUTFILE="$OUTDIR/drop_sweep_$STAMP.csv"

mkdir -p "$OUTDIR"

echo "timestamp,leader,mode,drop_percent,result,duration_ms" > "$OUTFILE"

run_case() {
  local drop="$1"
  local start end duration result output

  "$CLIENT" "$LEADER" "$PORT" drop "$drop" >/dev/null
  "$CLIENT" "$LEADER" "$PORT" mode quorum >/dev/null

  start="$(date +%s%3N)"

  output="$("$CLIENT" "$LEADER" "$PORT" append "drop sweep ${drop}%" 2>&1 || true)"

  if echo "$output" | grep -q "append succeeded"; then
    result="ok"
  else
    result="fail"
  fi

  end="$(date +%s%3N)"
  duration=$((end - start))

  echo "$(date -Iseconds),$LEADER,quorum,$drop,$result,$duration" >> "$OUTFILE"
  echo "drop=${drop}% result=$result duration=${duration}ms"
}

run_case 0
run_case 25
run_case 50
run_case 75
run_case 100

"$CLIENT" "$LEADER" "$PORT" drop 0 >/dev/null

echo
echo "Wrote results to $OUTFILE"