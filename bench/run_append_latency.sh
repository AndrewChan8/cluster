#!/usr/bin/env bash
set -uo pipefail

PORT=5000
TRIALS=5
RESULTS_DIR="bench/results"
OUT="$RESULTS_DIR/append_latency_$(date +%Y%m%d_%H%M%S).txt"

mkdir -p "$RESULTS_DIR"

run_timed() {
  local label="$1"
  shift

  echo "=== $label ===" | tee -a "$OUT"
  echo "command: $*" | tee -a "$OUT"

  ./bench/time_command.sh "$@" 2>&1 | tee -a "$OUT"

  echo "" | tee -a "$OUT"
}

run_mode_trials() {
  local mode="$1"

  run_timed "set mode $mode" ./rpc/client node1 "$PORT" mode "$mode"

  for i in $(seq 1 "$TRIALS"); do
    run_timed "$mode append trial $i" \
      ./rpc/client node1 "$PORT" append "bench ${mode} append trial ${i}"
  done
}

echo "Append latency benchmark" | tee -a "$OUT"
echo "Trials per mode: $TRIALS" | tee -a "$OUT"
echo "Output: $OUT" | tee -a "$OUT"
echo "" | tee -a "$OUT"

run_mode_trials strong
run_mode_trials quorum
run_mode_trials eventual

echo "Done. Results saved to $OUT"