# Adaptive Consistency Replicated Ledger

## Overview

This project looks at adaptive consistency mechanisms within a blockchain-like replicated ledger operating under failures and contention.

The system was implemented in C and evaluates whether dynamically changing commit requirements can improve recovery and convergence behavior compared to fixed consistency policies.

Implementation includes a custom TCP-based RPC framework, leader coordination, anti-entropy synchronization, and controlled failure injection.

## Motivation

Distributed systems frequently trade coordination overhead for availability and responsiveness.

This project investigates whether commit requirements should remain fixed or adapt automatically according to observed operating conditions.

Adaptive consistency is the primary mechanism evaluated in this work.

## Features

Implemented functionality includes:

- Custom TCP-based RPC communication
- Replicated append-only ledger
- Adaptive consistency transitions
- Eventual, quorum, and strong commit requirements
- Leader election and automatic failover
- Anti-entropy synchronization and recovery
- Failure injection and benchmark evaluation
- Distributed execution across physical Linux hosts

## System Architecture

Architecture summary:

```
Client
  ↓
Leader
  ↓
Replication
  ↓
Followers
  ↓
Anti-Entropy Recovery
```

Nodes maintain:

- replicated ledger state
- consistency behavior
- synchronization state
- failure recovery mechanisms

Leader election determines which node accepts write-oriented requests.

## Adaptive Consistency

Three commit requirements are supported:

### Eventual

Leader commits locally and followers synchronize asynchronously.

### Quorum

Commit requires majority acknowledgment.

### Strong

Commit requires acknowledgment from all replicas.

Adaptive mode automatically strengthens or relaxes commit requirements according to:

- replication failures
- missing acknowledgements
- contention conditions

## Repository Structure

```
cluster/
├── README.md
├── MANUAL.md
├── bench/
├── rpc/
└── screenshots/
```

Main directories:

- `rpc/` — implementation
- `rpc/scripts/` — validation scripts
- `bench/` — benchmark execution and outputs

## Quick Start

Build:

```
cd rpc
make
```

Run:

```
./server 5000
```

Client:

```
./client <host> <port> <command>
```

See `MANUAL.md` for complete execution instructions.

## Evaluation

Representative evaluation includes:

- append latency
- convergence time
- failover recovery
- packet drop experiments
- partition recovery
- adaptive consistency transitions

## Limitations

Current implementation intentionally simplifies several production concerns:

- full conflict reconciliation
- incremental synchronization
- dynamic membership
- large-scale deployment

Recovery currently uses full ledger synchronization.

## Additional Documentation

- `MANUAL.md` — build and usage instructions
- Project report — design and evaluation
- Slides — presentation material
- Screenshots / demo artifacts