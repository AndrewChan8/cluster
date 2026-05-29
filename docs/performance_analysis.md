# Performance Analysis

## Purpose

This document analyzes the performance and systems tradeoffs observed in the replicated ledger prototype.

The goal is to connect benchmark results and failure experiments to distributed systems concepts such as coordination overhead, availability, divergence, and recovery.

## Key Questions

This analysis focuses on:

- How does consistency mode affect append latency?
- How does coordination overhead change across strong, quorum, and eventual consistency?
- How do failures affect availability?
- When do replicas diverge?
- How does anti-entropy repair restore convergence?

## Append Latency Results

The append latency benchmark was run with all three nodes available.

| Mode | Trials | Latencies (ms) | Average Latency |
|---|---:|---|---:|
| Strong | 5 | 32, 23, 24, 22, 23 | 24.8 ms |
| Quorum | 5 | 16, 16, 16, 17, 16 | 16.2 ms |
| Eventual | 5 | 18, 17, 17, 17, 18 | 17.4 ms |

## Append Latency Analysis

Strong consistency had the highest average append latency because each append required a prepare phase followed by a commit phase across both followers.

Quorum and eventual consistency had lower append latency because they used a simpler replication path and did not require the same two-phase coordination structure.

These results support the expected tradeoff: stronger coordination increases latency, while weaker coordination improves responsiveness.

## Coordination Cost

Strong consistency requires more coordination messages than quorum or eventual consistency.

In strong mode, the leader must:

1. send prepare messages to followers
2. wait for acknowledgments
3. send commit messages
4. append only after coordination succeeds

In quorum and eventual modes, the leader uses a simpler replication path and can commit with fewer coordination requirements.

This explains why strong consistency produced higher append latency in the benchmark.

## Availability Tradeoffs

The failure experiments showed that consistency mode directly affects availability.

Strong consistency rejected writes when a follower was unavailable because the system could not complete full coordination.

Quorum consistency accepted writes when one follower was unavailable because the quorum condition could still be satisfied.

Eventual consistency accepted writes even when all followers were unavailable because local leader progress was decoupled from immediate follower coordination.

## Divergence Behavior

Replica divergence occurred when unavailable followers missed accepted writes.

Strong consistency avoided divergence by rejecting writes when full coordination was impossible.

Quorum consistency allowed limited divergence when one follower was unavailable.

Eventual consistency allowed the highest divergence risk because writes could succeed even when no followers acknowledged replication.

## Recovery and Anti-Entropy

The anti-entropy repair mechanism restored convergence by comparing replica status values.

The repair process used:

- ledger size
- last ledger hash

If the follower differed from the leader, the follower requested a full snapshot synchronization.

This restored convergence after temporary divergence.

## Automatic Anti-Entropy Repair

The automatic anti-entropy mechanism extends the manual repair functionality by continuously monitoring replica state in the background.

Follower nodes periodically compare their local ledger status against the leader using `size` and `last_hash`.

When divergence is detected, the follower automatically requests synchronization and repairs itself without requiring operator intervention.

This behavior improves system resilience and demonstrates a simple self-healing recovery mechanism commonly found in eventually consistent distributed systems.

The experiment showed that followers which missed writes during outages automatically converged back to the leader state after reconnecting.

## Summary

The benchmark and failure experiments show a consistent distributed systems tradeoff:

- strong consistency improves safety but increases latency and reduces availability
- quorum consistency balances availability and replication durability
- eventual consistency maximizes availability but increases temporary divergence risk
- anti-entropy repair restores convergence after failures

Overall, the system demonstrates how distributed replication protocols must balance coordination, latency, availability, and recovery complexity.