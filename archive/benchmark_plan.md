# Benchmark Plan

## Goal

Measure how different consistency modes affect latency, availability, divergence, and recovery behavior in the replicated ledger system.

## Metrics

- Append latency
- Append success/failure
- Follower acknowledgments
- Replica divergence
- Repair latency
- Convergence after repair

## Benchmark Scenarios

| Scenario | Mode | Failed Followers | Expected Result |
|---|---|---:|---|
| Strong all nodes up | strong | 0 | append succeeds |
| Strong one follower down | strong | 1 | append fails |
| Quorum one follower down | quorum | 1 | append succeeds |
| Eventual all followers down | eventual | 2 | append succeeds |
| Repair divergent follower | repair | 1 stale follower | follower converges |

## Planned Outputs

- latency table
- success/failure table
- convergence table
- summary analysis