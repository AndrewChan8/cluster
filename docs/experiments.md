# Experimental Evaluation

## Purpose

This document records the experimental evaluation of the replicated ledger system under different consistency modes and failure scenarios.

The goal is to compare how strong, quorum, and eventual consistency affect availability, divergence, convergence, and recovery behavior.

## Experimental Setup

- `node1`: leader
- `node2`: follower
- `node3`: follower
- Transport: TCP sockets
- Ledger model: append-only hash-linked replicated log
- Status metric: `size` and `last_hash`

## Metrics

The experiments evaluate:

- append success or failure
- replica availability
- ledger divergence
- convergence after repair
- follower acknowledgments
- recovery behavior

## Experiment 1: Strong Consistency with All Nodes Up

### Objective

Evaluate strong consistency behavior when all replicas are available.

### Procedure

1. Start `node1`, `node2`, and `node3`
2. Set consistency mode to `strong`
3. Append a transaction through the leader
4. Query replica status on all nodes

Commands:

```
./client node1 5000 mode strong
./client node1 5000 append "exp strong all up"
./client node1 5000 status
./client node2 5000 status
./client node3 5000 status
```

### Observations

- The append operation succeeded
- All followers acknowledged the prepare phase
- All replicas converged to the same ledger state
- Replica hashes and sizes matched across all nodes
- No divergence occurred

Example result:
```
Mode=0 follower_acks=2/2
size=2 last_hash=00000000000000000000000000000000000000000000000018f70a3495f1bf9a
```

Note: The exact `last_hash` value may differ between runs depending on the ledger contents and append ordering.

### Analysis

Strong consistency successfully prevented divergence by requiring all replicas to acknowledge before commit.

This behavior maximized consistency and safety, but required every follower to remain available before writes could succeed.

The experiment demonstrated that strong consistency preserves a single globally consistent replicated state when all nodes are operational.

The experiment also demonstrates the classic distributed systems tradeoff between consistency and availability. Strong consistency ensures that replicas never diverge, but any unavailable follower can block progress for the entire system.

## Experiment 2: Strong Consistency with Follower Down

### Objective

Evaluate strong consistency behavior when a follower replica becomes unavailable.

### Procedure

1. Start `node1` and `node2`
2. Leave `node3` offline
3. Set consistency mode to `strong`
4. Attempt to append a transaction through the leader
5. Query replica status on the remaining nodes

Commands:

```
./client node1 5000 mode strong
./client node1 5000 append "failtest strong node3 down"
./client node1 5000 status
./client node2 5000 status
```

### Observations

- The append operation failed
- The leader could not contact `node3`
- The prepare phase did not complete successfully
- Abort messages were issued to followers
- No partial commit occurred
- Replica hashes and sizes remained unchanged

Example result:
```
append error: strong mode prepare failed
Mode=0 follower_acks=1/2
```

### Analysis

Strong consistency prevented the system from committing a partially replicated transaction when one follower became unavailable.

Although `node1` and `node2` were still operational, the system rejected the write because all replicas were required to acknowledge the prepare phase before commit.

This experiment demonstrates that strong consistency prioritizes safety and consistency over availability during failures.

The experiment also illustrates an important distributed systems principle: preventing divergence often requires sacrificing progress when coordination cannot be guaranteed across all replicas.

The prepare/commit/abort flow used in this experiment resembles simplified two-phase commit coordination semantics.

## Experiment 3: Quorum Consistency with One Follower Down

### Objective

Evaluate quorum consistency behavior when one follower replica becomes unavailable.

### Procedure

1. Start `node1` and `node2`
2. Leave `node3` offline
3. Set consistency mode to `quorum`
4. Append a transaction through the leader
5. Query replica status on available nodes

Commands:

```
./client node1 5000 mode quorum
./client node1 5000 append "failtest quorum node3 down"
./client node1 5000 status
./client node2 5000 status
```

### Observations

- The append operation succeeded
- The leader could not contact `node3`
- Replication to `node2` succeeded
- The quorum condition was satisfied with one follower acknowledgment
- `node1` and `node2` converged to the same ledger state
- `node3` temporarily diverged from the cluster state

Example result:
```
Mode=1 follower_acks=1/2
append succeeded
```

### Analysis

Quorum consistency allowed the system to continue making progress despite one unavailable follower replica.

Unlike strong consistency, quorum mode did not require all followers to acknowledge replication before committing the transaction.

This behavior improved system availability while still maintaining partial replication durability through the remaining active follower.

The experiment demonstrates the tradeoff between consistency and availability in distributed systems. Quorum-based coordination allows systems to tolerate partial failures while accepting the possibility of temporary replica divergence.

This experiment also illustrates how quorum semantics provide a middle ground between strict coordination and fully asynchronous eventual consistency.

This behavior resembles many real-world distributed systems that continue operating under partial replica failures while relying on later synchronization to restore convergence.

## Experiment 4: Eventual Consistency with Followers Down

### Objective

Evaluate eventual consistency behavior when follower replicas are unavailable.

### Procedure

1. Start only `node1`
2. Leave `node2` and `node3` offline
3. Set consistency mode to `eventual`
4. Append a transaction through the leader
5. Query leader status

Commands:

```
./client node1 5000 mode eventual
./client node1 5000 append "failtest eventual followers down"
./client node1 5000 status
```

### Observations

- The append operation succeeded
- The leader could not contact `node2`
- The leader could not contact `node3`
- No follower acknowledgments were received
- The leader still committed the append locally
- Followers became fully divergent from the leader state

Example result:
```
Mode=2 follower_acks=0/2
append succeeded
```

### Analysis

Eventual consistency allowed the system to continue accepting writes despite all follower replicas being unavailable.

Unlike strong consistency and quorum consistency, the leader did not require any successful follower acknowledgments before committing the transaction locally.

This behavior maximized system availability and allowed progress even during severe replica failures.

However, the experiment also produced the highest divergence risk because follower replicas no longer shared a consistent replicated state with the leader.

The experiment demonstrates the core tradeoff of eventual consistency systems: temporary inconsistency is tolerated in exchange for continued availability and write progress under failures.

This behavior resembles large-scale distributed systems that prioritize uptime and asynchronous convergence over immediate global consistency.

The experiment demonstrates how eventual consistency systems decouple write availability from immediate replica coordination.

## Experiment 5: Anti-Entropy Repair

### Objective

Evaluate anti-entropy repair behavior after follower divergence.

### Procedure

1. Start only `node1` and `node3`
2. Leave `node2` offline
3. Set consistency mode to `eventual`
4. Append a transaction through the leader
5. Restart `node2`
6. Compare replica status values
7. Trigger repair synchronization from `node2`
8. Verify replica convergence after repair

Commands:

```
./client node1 5000 mode eventual
./client node1 5000 append "repair needed test"
./client node1 5000 status

./client node2 5000 status
./client node1 5000 status

./client node2 5000 repair node1

./client node2 5000 status
```

### Observations

- `node2` initially diverged from the leader state
- Replica hashes and sizes differed before repair
- The repair operation detected divergence automatically
- `node2` requested synchronization from the leader
- The follower replaced its local ledger with the leader snapshot
- Replica hashes and sizes matched after repair
- Convergence was restored successfully

Example result:
```
local_before="size=2 last_hash=..."
leader="size=3 last_hash=..."
repaired=1
```

### Analysis

The anti-entropy repair mechanism successfully detected divergence and restored replica convergence after follower outage recovery.

Unlike the earlier experiments, this evaluation demonstrated not only divergence creation but also automated convergence restoration through synchronization.

The repair mechanism used replica status metadata (size and last_hash) to determine whether synchronization was necessary before requesting a full ledger snapshot from the leader.

This experiment demonstrates an important distributed systems principle: eventual consistency systems rely on background synchronization and anti-entropy mechanisms to repair temporary inconsistency over time.

The experiment also illustrates how distributed systems can prioritize availability during failures while still eventually converging toward a consistent replicated state after recovery.

## Summary of Findings

### Key Findings

The experiments demonstrated clear tradeoffs between consistency, availability, divergence tolerance, and recovery behavior across different replication semantics.

Strong consistency preserved a globally synchronized replicated state by requiring all followers to participate in coordination before commit. This approach maximized consistency and safety but reduced system availability during failures.

Quorum consistency provided a balance between strict coordination and availability by allowing progress with partial follower participation. This improved fault tolerance while still maintaining partial replication durability.

Eventual consistency maximized write availability by decoupling local commit behavior from immediate replica coordination. However, this increased temporary divergence risk across replicas during outages.

The anti-entropy repair mechanism demonstrated that divergent replicas could later reconverge through synchronization and snapshot-based recovery.

Overall, the experiments showed that distributed systems must continuously balance:

- consistency
- availability
- coordination overhead
- failure tolerance
- recovery complexity

The project also demonstrated that no single replication strategy is universally optimal. Different consistency semantics provide different tradeoffs depending on system goals, failure assumptions, and operational priorities.

These experiments reflect many of the same coordination and replication tradeoffs encountered in real-world distributed systems, databases, replicated storage systems, and blockchain-style architectures.