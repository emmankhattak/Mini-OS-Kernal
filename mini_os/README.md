# Mini OS Kernel Simulator

A fully modular C++17 simulation of core operating-system subsystems:
CPU scheduling, memory management, thread control, and synchronisation primitives.

---

## Quick Start

```bash
make          # compile
make run      # compile + execute
make clean    # remove binary
```

Requires **g++ with C++17** support (`-std=c++17`).

---

## File Map — Where Everything Lives

```
mini_os/
├── Makefile
├── README.md          ← you are here
│
├── include/           ← all headers (header-only except scheduler_io)
│   ├── config.h           global constants  (frame count, quantum, etc.)
│   ├── enums.h            ProcessState, ThreadState, SchedulerType + converters
│   ├── tcb.h              Thread Control Block  (struct TCB)
│   ├── pcb.h              Process Control Block (class PCB, owns TCB list)
│   ├── memory_manager.h   demand paging + FIFO page replacement
│   ├── sync.h             SimMutex, SimSemaphore, IORequest
│   ├── gantt_chart.h      ASCII Gantt chart renderer
│   ├── analytics.h        per-run metrics + comparative table
│   ├── scheduler.h        FCFS / RR / SJF / Priority + helpers
│   └── display_utils.h    banner, workload table, config dump
│
└── src/
    ├── main.cpp            workload definitions + simulation driver
    └── scheduler_io.cpp    runRoundRobinIO() — RR + I/O + Mutex + TCB
```

---

## Include Dependency Chain (no cycles)

```
config.h
  └── enums.h
        └── tcb.h
              └── pcb.h
                    ├── memory_manager.h
                    ├── sync.h
                    ├── gantt_chart.h
                    └── analytics.h
                          └── scheduler.h
                                ├── display_utils.h
                                └── [scheduler_io.cpp implements runRoundRobinIO()]
```

---

## Algorithms Implemented

| # | Algorithm | Preemptive | Data Structure | Best For |
|---|-----------|-----------|----------------|----------|
| 1 | FCFS | No | FIFO queue | Simple batch jobs |
| 2 | Round Robin | Yes | Circular FIFO queue | Time-sharing systems |
| 3 | SJF | No | Min-heap (remaining_time) | Minimising avg wait |
| 4 | Priority | No | Min-heap (priority value) | Real-time tasks |
| 5 | RR + I/O + Mutex | Yes | FIFO + IORequest list | Mixed I/O workloads |

---

## Key Metrics Computed (AnalyticsEngine)

| Metric | Formula |
|--------|---------|
| Waiting Time (WT) | `TAT − burst_time` |
| Turnaround Time (TAT) | `completion_time − arrival_time` |
| Response Time | `first_dispatch − arrival_time` |
| CPU Utilisation | `(total − idle) / total × 100` |
| Throughput | `n / total_time` |

---

## Memory Management Summary

- **Policy:** Demand Paging — pages are loaded only on first access.
- **Replacement:** FIFO — the oldest loaded frame is evicted when RAM is full.
- **Physical RAM:** 16 frames × 256 bytes = 4 096 bytes.
- **Per-process:** up to 8 virtual pages; page table maps logical → physical frame.
- **Page fault path:** `accessMemory()` → `handlePageFault()` → `evictFIFO()` (if needed).

---

## Synchronisation Summary

| Primitive | Type | Key Methods | File |
|-----------|------|-------------|------|
| `SimMutex` | Binary lock | `acquire()`, `release()`, `enqueueWaiter()` | `sync.h` |
| `SimSemaphore` | Counting lock | `wait_op()`, `signal_op()`, `enqueueWaiter()` | `sync.h` |
| `IORequest` | I/O descriptor | struct: pid, finish_tick, io_type | `sync.h` |

---

## Thread Model

- Every `PCB` automatically creates one **main TCB** at construction (carrying the full burst).
- Additional threads can be spawned with `PCB::addThread(arrival, burst)`.
- TCBs share the parent's page table (user-level thread model).
- `tickThreads()` in the scheduler decrements the running TCB's `remaining_time` each clock tick.
- When a TCB's `remaining_time` reaches 0, `calculateMetrics()` is called and state → `TERMINATED`.

Developer:
Emman Khattak
NUST MCS