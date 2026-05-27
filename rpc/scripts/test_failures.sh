#!/usr/bin/env bash
set -euo pipefail

PORT=5000
RESULTS_DIR="results"
OUT="$RESULTS_DIR/failure_experiment_checklist_$(date +%Y%m%d_%H%M%S).txt"

mkdir -p "$RESULTS_DIR"

cat > "$OUT" <<EOF
# Failure Experiment Checklist

# Preconditions:
# - Build latest code on all nodes.
# - Start node1 server.
# - Start/stop node2 and node3 as directed.
# - Use separate terminals for servers.

========================================
1. STRONG MODE: follower down -> reject
========================================

Start:
  node1 server running
  node2 server running
  node3 server STOPPED

Run:
  ./client node1 $PORT mode strong
  ./client node1 $PORT append "failtest strong node3 down"
  ./client node1 $PORT status
  ./client node2 $PORT status

Expected:
  append error: strong mode prepare failed
  node1 and node2 should not append the new entry

========================================
2. QUORUM MODE: one follower down -> accept
========================================

Start:
  node1 server running
  node2 server running
  node3 server STOPPED

Run:
  ./client node1 $PORT mode quorum
  ./client node1 $PORT append "failtest quorum node3 down"
  ./client node1 $PORT status
  ./client node2 $PORT status

Expected:
  append succeeded
  node1 and node2 same size/hash
  node3 missing entry until sync

========================================
3. EVENTUAL MODE: followers down -> accept
========================================

Start:
  node1 server running
  node2 server STOPPED
  node3 server STOPPED

Run:
  ./client node1 $PORT mode eventual
  ./client node1 $PORT append "failtest eventual followers down"
  ./client node1 $PORT status

Expected:
  append succeeded
  Mode=2 follower_acks=0/2 on leader
  followers are divergent/missing entry

========================================
4. RECOVERY: followers sync from leader
========================================

Start:
  restart node2 server
  restart node3 server

Run:
  ./client node1 $PORT status
  ./client node2 $PORT status
  ./client node3 $PORT status
  ./client node2 $PORT sync node1
  ./client node3 $PORT sync node1
  ./client node1 $PORT status
  ./client node2 $PORT status
  ./client node3 $PORT status

Expected:
  before sync: follower hashes differ from leader
  after sync: all size/hash values match
EOF

echo "Wrote failure experiment checklist to $OUT"