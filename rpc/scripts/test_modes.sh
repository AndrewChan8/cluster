#!/usr/bin/env bash
set -euo pipefail

PORT=5000
RESULTS_DIR="results"
LOG="$RESULTS_DIR/experiment_$(date +%Y%m%d_%H%M%S).txt"

mkdir -p "$RESULTS_DIR"

run_and_log() {
  echo ">>> $*" | tee -a "$LOG"
  "$@" 2>&1 | tee -a "$LOG"
  echo "" | tee -a "$LOG"
}

echo "Experiment log: $LOG" | tee -a "$LOG"
echo "" | tee -a "$LOG"

echo "=== STATUS SNAPSHOT ===" | tee -a "$LOG"
run_and_log ./client node1 "$PORT" status
run_and_log ./client node2 "$PORT" status
run_and_log ./client node3 "$PORT" status

echo "=== STRONG MODE TEST ===" | tee -a "$LOG"
run_and_log ./client node1 "$PORT" mode strong
run_and_log ./client node1 "$PORT" append "exp strong all up"
run_and_log ./client node1 "$PORT" status
run_and_log ./client node2 "$PORT" status
run_and_log ./client node3 "$PORT" status

echo "=== QUORUM MODE TEST ===" | tee -a "$LOG"
run_and_log ./client node1 "$PORT" mode quorum
run_and_log ./client node1 "$PORT" append "exp quorum test"
run_and_log ./client node1 "$PORT" status
run_and_log ./client node2 "$PORT" status
run_and_log ./client node3 "$PORT" status

echo "=== EVENTUAL MODE TEST ===" | tee -a "$LOG"
run_and_log ./client node1 "$PORT" mode eventual
run_and_log ./client node1 "$PORT" append "exp eventual test"
run_and_log ./client node1 "$PORT" status
run_and_log ./client node2 "$PORT" status
run_and_log ./client node3 "$PORT" status

echo "=== RECOVERY SYNC TEST ===" | tee -a "$LOG"
run_and_log ./client node2 "$PORT" sync node1
run_and_log ./client node3 "$PORT" sync node1
run_and_log ./client node1 "$PORT" status
run_and_log ./client node2 "$PORT" status
run_and_log ./client node3 "$PORT" status

echo "Done. Results saved to $LOG"