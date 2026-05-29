# Distributed Ledger Replication System

## Overview

This project is an experimental distributed systems prototype that explores replicated append-only ledger consistency semantics under failures and network uncertainty.

The system implements a TCP-based replicated ledger with multiple consistency modes, replication protocols, recovery synchronization, persistent storage, and fault experiments across multiple physical nodes.

The primary goal of the project is to study the tradeoffs between consistency, availability, divergence, and recovery behavior in distributed replicated systems.

The implementation is written in C and uses a custom RPC-style message protocol over TCP sockets for communication between clients, leaders, and followers.

Technologies used:
- C
- POSIX sockets
- TCP networking
- Linux
- Distributed replication protocols

## Goals

The primary objectives of this project are:

- Explore strong, quorum, and eventual consistency semantics
- Observe distributed system behavior under follower failures
- Study divergence and convergence in replicated ledgers
- Evaluate recovery synchronization after node outages
- Compare consistency and availability tradeoffs
- Experiment with replicated append-only ledger design
- Build a fault-tolerant distributed systems prototype across multiple physical nodes

## System Architecture

The system is organized as a replicated leader/follower distributed ledger.

- `node1` acts as the leader node
- `node2` and `node3` act as follower replicas
- Clients send requests to the leader
- The leader coordinates replication across followers
- Followers store replicated ledger entries locally

The system uses a TCP-based RPC-style communication layer with framed messages containing:

- message type
- request ID
- payload length
- payload data

Ledger entries are append-only and hash-linked to previous entries, forming a SHA-256 hash-linked append-only ledger structure.

Each node maintains:

- a local persistent ledger
- replication handlers
- synchronization logic
- consistency mode behavior
- recovery mechanisms

The architecture supports:

- replicated append operations
- follower synchronization
- runtime consistency switching
- divergence detection
- manual recovery synchronization
- automatic anti-entropy repair
- periodic anti-entropy verification
- self-healing follower recovery

## Features

The current prototype includes:

- TCP client/server communication
- Custom framed message protocol
- Append-only replicated ledger
- SHA-256 hash-linked ledger entries using OpenSSL
- Persistent ledger storage on disk
- Fixed leader/follower deployment model
- Runtime consistency mode switching
- Strong consistency prepare/commit/abort flow
- Quorum-based append behavior
- Eventual consistency append behavior
- Follower catch-up synchronization
- Ledger status queries using `size` and `last_hash`
- Divergence detection through hash comparison
- Experiment scripts for normal and failure scenarios
- Status-based anti-entropy repair
- Automatic background anti-entropy repair
- Self-healing follower convergence after outages

## Consistency Modes

The system supports multiple replication consistency semantics that can be changed at runtime.

### Strong Consistency

Strong consistency requires all follower replicas to successfully prepare before a write is committed.

Behavior:
- leader sends prepare requests
- all followers must acknowledge
- leader commits only if all followers respond successfully
- failed prepares trigger abort messages

Tradeoff:
- prioritizes consistency and safety
- sacrifices availability during failures

### Quorum Consistency

Quorum consistency allows writes to succeed if at least one follower successfully acknowledges replication.

Behavior:
- leader replicates to followers
- write succeeds if quorum condition is satisfied
- unavailable followers may temporarily diverge

Tradeoff:
- balances consistency and availability
- tolerates partial follower failures

### Eventual Consistency

Eventual consistency allows the leader to accept writes immediately even if followers are unavailable.

Behavior:
- leader appends locally
- follower replication is attempted opportunistically
- replicas may temporarily diverge
- followers recover later through synchronization

Tradeoff:
- maximizes availability
- allows temporary inconsistency and divergence

## Failure Recovery

The system supports manual follower recovery synchronization.

When a follower falls behind or becomes unavailable, it may later request a full ledger synchronization from the leader.

Recovery process:
1. follower reconnects to the system
2. follower sends a sync request to the leader
3. leader serializes its current ledger state
4. follower replaces its local ledger with the leader snapshot
5. replicas converge to the same ledger hash and size

The recovery mechanism allows:
- divergence detection
- replica convergence
- follower catch-up after outages
- restoration of consistent replicated state

Replica consistency can be verified using:
- ledger size
- last ledger hash

The system also supports automatic anti-entropy repair on follower nodes.

Followers periodically compare their local ledger status against the leader using `size` and `last_hash`. If a follower detects divergence, it automatically requests synchronization and repairs itself without requiring a manual client command.

This allows stale followers to converge automatically after outages.

## Message Protocol

The system uses a custom framed TCP message protocol for all client and inter-node communication.

Each message contains:

- `type`
- `request_id`
- `length`
- `payload`

The protocol supports:

| Message Type | Purpose |
|---|---|
| `MSG_APPEND` | client append request |
| `MSG_REPL_APPEND` | follower replication |
| `MSG_PREPARE_APPEND` | strong consistency prepare phase |
| `MSG_COMMIT_APPEND` | strong consistency commit phase |
| `MSG_ABORT` | abort failed strong consistency operation |
| `MSG_GET_LOG` | retrieve serialized ledger |
| `MSG_SET_MODE` | switch consistency mode |
| `MSG_SYNC_REQUEST` | follower recovery synchronization |
| `MSG_SYNC_RESPONSE` | leader ledger snapshot response |
| `MSG_STATUS` | replica status query |
| `MSG_STATUS_RESPONSE` | replica hash/size response |
| `MSG_REPAIR` | status-based anti-entropy repair request |
| `MSG_REPAIR_RESPONSE` | repair result response |

The protocol uses explicit payload lengths and request IDs to support reliable framed communication over TCP sockets.

## Repository Structure

```
cluster/
├── README.md
├── bench/
│   ├── results/
│   ├── run_append_latency.sh
│   ├── time_command.sh
├── docs/
├── rpc/
│   ├── Makefile
│   ├── client.c
│   ├── server.c
│   ├── anti_entropy.c / anti_entropy.h
│   ├── common.c / common.h
│   ├── ledger.c / ledger.h
│   ├── replication.c / replication.h
│   ├── handlers.c / handlers.h
│   ├── server_context.h
│   └── scripts/
│       ├── test_modes.sh
│       └── test_failures.sh
└── scripts/
```

Main directories:

- `rpc/`: core distributed ledger implementation
- `rpc/scripts/`: experiment and failure-testing scripts
- `docs/`: experiment notes and evaluation results
- `bench/`: benchmarking scripts and performance measurements

## Building

Install OpenSSL development headers before building:

```
sudo apt install libssl-dev
```

The server links against OpenSSL `libcrypto` for SHA-256 hashing.

Build the project from the `rpc/` directory:

```
cd rpc
make
```
This produces:

- `server`
- `client`

Clean build artifacts using:
```
make clean
```

## Running the System

Start a server on each node:

### Leader

```
cd rpc
./server 5000
```

`node1` automatically becomes the leader node.

### Followers

Run the same command on follower nodes:

```
cd rpc
./server 5000
```

`node2` and `node3` automatically operate as followers.

Clients communicate with the leader using:

```
./client <host> <port> <command>
```

Example:

```
./client node1 5000 append "hello world"
```

## Example Commands

Append a transaction:

```
./client node1 5000 append "example transaction"
```

Retrieve the ledger:

```
./client node1 5000 log
```

Check replica status:

```
./client node1 5000 status
./client node2 5000 status
./client node3 5000 status
```

Switch consistency modes:

```
./client node1 5000 mode strong
./client node1 5000 mode quorum
./client node1 5000 mode eventual
```

Recover followers from the leader:

```
./client node2 5000 sync node1
./client node3 5000 sync node1
```

Repair a follower only if it differs from the leader:
```
./client node2 5000 repair node1
```

Automatic anti-entropy repair runs in the background on follower nodes:

```
anti-entropy: divergence detected
anti-entropy: local=size=0 last_hash=GENESIS
anti-entropy: leader=size=1 last_hash=...
Recovered ledger from leader:
anti-entropy: repair completed
```

## Experimental Evaluation

The project includes experiments for comparing consistency behavior under normal operation and failure scenarios.

The experimental evaluation includes:

- strong consistency with all nodes available
- strong consistency with follower failure
- quorum consistency with follower failure
- eventual consistency with follower outages
- anti-entropy repair after divergence

Experiment scripts are included in:

```
rpc/scripts/test_modes.sh
rpc/scripts/test_failures.sh
```

The normal-mode experiment records replica status after append operations under each consistency mode.

Replica convergence is evaluated using:

- ledger size
- last ledger hash

These experiments demonstrate the tradeoffs between consistency, availability, divergence, and recovery behavior under distributed failures.

## Benchmark Summary

Append latency measurements were collected across all consistency modes.

| Mode | Average Latency |
|--------|--------:|
| Strong | 24.8 ms |
| Quorum | 16.2 ms |
| Eventual | 17.4 ms |

The benchmark results demonstrate the classic distributed systems tradeoff between consistency and performance. Strong consistency incurred the highest latency because writes required full follower coordination. Quorum and eventual consistency reduced coordination overhead and therefore completed writes more quickly.

## Future Work

Potential future improvements include:

- leader election and failover
- logical clock integration
- conflict resolution mechanisms
- larger-scale logical node simulation
- configurable cluster membership
- network partition simulation
- extended throughput and scalability benchmarking
- automatic fault injection
- performance measurements under load
- consensus-based commit protocols
- visualization and monitoring dashboards
- configurable anti-entropy interval

The current implementation focuses primarily on consistency semantics, replication behavior, divergence, and recovery in distributed replicated systems.

Current implementation uses full snapshot anti-entropy repair.
Future versions could support incremental log-based repair.