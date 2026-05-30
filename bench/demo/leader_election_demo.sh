#!/usr/bin/env bash
set -u

CLIENT="./rpc/client"
PORT="5000"

run() {
  echo
  echo "=== $* ==="
  "$@"
}

echo "Distributed ledger demo"
echo "Assumes servers are already running on node1/node2/node3"
echo "Expected current leader: node2 or node3"

run "$CLIENT" node1 "$PORT" status
run "$CLIENT" node2 "$PORT" status
run "$CLIENT" node3 "$PORT" status

echo
echo "1) Test follower rejection"
run "$CLIENT" node1 "$PORT" append "follower should reject"

echo
echo "2) Set leader to strong mode"
run "$CLIENT" node2 "$PORT" mode strong

echo
echo "3) Strong append with all nodes up"
run "$CLIENT" node2 "$PORT" append "demo strong append"

run "$CLIENT" node1 "$PORT" log
run "$CLIENT" node2 "$PORT" log
run "$CLIENT" node3 "$PORT" log

echo
echo "Now manually stop one follower, then press ENTER."
read -r

echo
echo "4) Strong append should fail with missing follower"
run "$CLIENT" node2 "$PORT" append "demo strong should fail"

echo
echo "5) Quorum append should succeed with one follower"
run "$CLIENT" node2 "$PORT" mode quorum
run "$CLIENT" node2 "$PORT" append "demo quorum append"

echo
echo "6) Eventual append should succeed even with followers down"
run "$CLIENT" node2 "$PORT" mode eventual
run "$CLIENT" node2 "$PORT" append "demo eventual append"

echo
echo "Restart stopped follower, wait for anti-entropy repair, then press ENTER."
read -r

run "$CLIENT" node1 "$PORT" log
run "$CLIENT" node2 "$PORT" log
run "$CLIENT" node3 "$PORT" log

echo
echo "Now stop the leader, wait for new election, then press ENTER."
read -r

echo
echo "7) Test new leader after failover"
run "$CLIENT" node3 "$PORT" mode quorum
run "$CLIENT" node3 "$PORT" append "demo after failover"
run "$CLIENT" node3 "$PORT" log

echo
echo "Demo complete."