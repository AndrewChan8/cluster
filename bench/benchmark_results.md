# Benchmark Results

## Append Latency: All Nodes Up

| Mode | Trials | Latencies (ms) | Average Latency |
|---|---:|---|---:|
| Strong | 5 | 32, 23, 24, 22, 23 | 24.8 ms |
| Quorum | 5 | 16, 16, 16, 17, 16 | 16.2 ms |
| Eventual | 5 | 18, 17, 17, 17, 18 | 17.4 ms |

## Analysis

Strong consistency had the highest average append latency because each append required a prepare phase followed by a commit phase across both followers.

Quorum and eventual consistency had lower append latency because they used a simpler replication path and did not require the same two-phase coordination structure.

These results support the expected tradeoff: stronger coordination increases latency, while weaker coordination improves responsiveness.