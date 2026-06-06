# Manual

## Purpose

This project evaluates adaptive consistency mechanisms within a replicated blockchain-like ledger under failures and contention.

## Build

Build from the `rpc/` directory:

```
cd rpc
make clean
make
```

For a full reset of build artifacts and local ledger state:

```
make distclean
```

`make distclean` performs a normal clean operation and additionally removes persisted ledger state (`ledger.data`) to return the system to a fresh execution state.

Requirements:

- Debian Linux
- GCC
- OpenSSL development libraries

Install dependencies:

```
sudo apt install libssl-dev
```

## Cluster Setup

Start one server process on each node:

```
cd rpc
./server 5000
```

Wait for leader election before issuing client requests.

## Client Commands

Append:

```
./client node1 5000 append "example"
```

Status:

```
./client node1 5000 status
```

Consistency:

```
./client node1 5000 mode strong
./client node1 5000 mode quorum
./client node1 5000 mode eventual
./client node1 5000 adaptive on
```

Failure Injection:

```
./client node1 5000 latency <ms>
./client node1 5000 drop <percent>
./client node1 5000 partition <node>
./client node1 5000 heal
```

Recovery:

```
./client node2 5000 sync node1
./client node2 5000 repair node1
```

### Leader-Controlled Operations

Write-oriented commands such as:

- `append`
- `mode`
- `adaptive`
- `latency`
- `drop`
- `partition`
- `heal`

must be sent to the current leader node.

Follower replicas reject these requests and only participate in replication, synchronization, and recovery behavior.

Use:

```
./client <node> 5000 status
```

to determine the current leader before issuing leader-controlled commands.

## Benchmark Execution

Available scripts:

```
./bench/run_append_latency.sh
./bench/run_latency_sweep.sh
./bench/run_drop_sweep.sh
./bench/run_failover_time.sh
./bench/run_convergence_time.sh
```

## Expected Behavior

Adaptive consistency automatically adjusts commit requirements according to replication outcomes and contention conditions.

Temporary replica divergence may occur during failures.

Follower recovery occurs through anti-entropy synchronization.
