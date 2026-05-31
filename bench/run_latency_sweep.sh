#!/usr/bin/env bash
set -euo pipefail

CLIENT="./rpc/client"
PORT="5000"
LEADER="${1:-node2}"
OUTDIR="bench/results"
STAMP="$(date +%Y%m%d_%H%M%S)"
OUTFILE="$OUTDIR/latency_sweep_$STAMP.csv"

mkdir -p "$OUTDIR"

echo "timestamp,leader,mode,latency_ms,result,duration_ms" > "$OUTFILE"

run_case() {
  local latency_ms="$1"
  local start
  local end
  local duration
  local result

  "$CLIENT" "$LEADER" "$PORT" latency "$latency_ms" >/dev/null
  "$CLIENT" "$LEADER" "$PORT" mode strong >/dev/null

  start="$(date +%s%3N)"

  if "$CLIENT" "$LEADER" "$PORT" append "latency sweep ${latency_ms}ms" >/dev/null; then
    result="ok"
  else
    result="fail"
  fi

  end="$(date +%s%3N)"
  duration=$((end - start))

  echo "$(date -Iseconds),$LEADER,strong,$latency_ms,$result,$duration" >> "$OUTFILE"
  echo "latency=${latency_ms}ms result=$result duration=${duration}ms"
}

run_case 0
run_case 250
run_case 500
run_case 1000

"$CLIENT" "$LEADER" "$PORT" latency 0 >/dev/null

echo
echo "Wrote results to $OUTFILE"