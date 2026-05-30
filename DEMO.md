# Distributed Ledger Demo

This demo showcases a distributed replicated ledger supporting:

- Leader election and failover
- Strong, quorum, and eventual consistency
- Replicated transaction logs
- Automatic anti-entropy recovery
- Leader replacement after node failure

## Setup

Run on all nodes before the demo:

```
cd ~/cluster/rpc
rm -f ledger.data
make clean && make
```

Start servers:
```
./server 5000
```

Recommended startup order:
```
node1
node2
node3
```

Wait until a leader election completes before running the demo.

## Demo

Run from node1:
```
cd ~/cluster
./bench/demo/leader_election_demo.sh
```

## What the Demo Shows

- All nodes begin with empty ledgers.
- A follower rejects client writes.
- The elected leader accepts writes.
- Strong consistency replicates to all available nodes.
- Strong mode fails when a follower is unavailable.
- Quorum mode succeeds with one follower unavailable.
- Eventual mode accepts writes despite missing replicas.
- Restarted nodes automatically recover using anti-entropy.
- When the leader fails, a new leader is elected.
- The new leader accepts writes.
- Log entries created after failover use the new election term.

## Known Limitations

- The demo script assumes node2 wins the first election.
- The demo script assumes node3 wins after node2 is stopped.
- Consistency mode is runtime-local and resets to strong on restart or leadership change.
- Clients manually send requests to the leader.
- The election system is Raft-inspired but does not implement full Raft log replication semantics.
- Anti-entropy assumes a reachable leader is known by followers.