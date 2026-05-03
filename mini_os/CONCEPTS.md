# VIVA NOTES — Mini OS Kernel Simulator
# Read this the night before. Each Q has a 3-line verbal answer + the exact file to point at.

---

## SECTION 1: Process Control Block (PCB)

**Q: What is a PCB and what does it store?**
A PCB is the OS's complete data record for one process. It stores identity (PID, name),
scheduling fields (arrival, burst, remaining, priority), memory info (page table, fault count),
and execution history (log). In our code it also holds the TCB list for its threads.
→ FILE: `include/pcb.h`

**Q: Why do we need a PCB?**
The OS must context-switch between many processes. The PCB is the "save slot" — when a process
is preempted, all its state is already in the PCB, so the scheduler can resume it later without
any data loss.

**Q: What is response_time and how is it set?**
Response time = time from arrival to first CPU dispatch. It is initialised to -1 and set
exactly once, when the process is first dequeued and put into RUNNING state.
→ `pcb.h`: field `response_time`; set in each scheduler's dequeue block.

**Q: What does PCB::reset() do and why is it needed?**
Before running a new scheduling algorithm we call reset() on every PCB. It clears all
metrics (waiting_time, turnaround_time, completion_time), resets remaining_time = burst_time,
reinitialises the page table to all -1, and rebuilds the TCB list from scratch.
→ `include/pcb.h` → `reset()`

---

## SECTION 2: Thread Control Block (TCB)

**Q: What is a TCB and how does it differ from a PCB?**
A TCB (Thread Control Block) is a lighter-weight record for one thread within a process.
It stores its own tid, state, burst/remaining/waiting times, and log. Unlike a PCB it does
NOT own memory — it shares the parent PCB's page table (user-level thread model).
→ FILE: `include/tcb.h`

**Q: How is the main thread created?**
The PCB constructor calls `addThread(arrival_time, burst_time)` automatically. This creates
TCB with tid = pid×100 + 0, name = "Pn-T0", carrying the full burst of the process.
→ `include/pcb.h` → constructor → `addThread()`

**Q: How are TCB metrics calculated?**
`TCB::calculateMetrics(comp_time)` sets:
  completion_time = comp_time
  turnaround_time = completion_time − arrival_time
  waiting_time    = max(0, turnaround_time − burst_time)
→ `include/tcb.h` → `calculateMetrics()`

**Q: What is tickThreads() and where is it called?**
`tickThreads()` is a private Scheduler helper called once per clock tick inside
`runRoundRobinIO()`. It decrements the `remaining_time` of any TCB in RUNNING state,
and when it hits 0, calls `calculateMetrics()` and transitions the TCB to TERMINATED.
→ `include/scheduler.h` → `tickThreads()` (private)

---

## SECTION 3: Scheduling Algorithms

**Q: Explain FCFS.**
First-Come-First-Serve is non-preemptive. Processes enter a FIFO queue in arrival order.
The CPU runs each process to completion before picking the next. Simple, fair by arrival,
but suffers the Convoy Effect — short jobs behind long jobs wait unnecessarily.
→ `include/scheduler.h` → `runFCFS()`

**Q: What is the Convoy Effect?**
When a long CPU-bound process holds the CPU, many short processes pile up behind it,
inflating their waiting time. FCFS is the worst algorithm for this — SJF eliminates it.

**Q: Explain Round Robin.**
RR is preemptive. Processes share the CPU in fixed time slices (quantum = 3 units here).
After each quantum the running process is moved to the back of the ready queue.
Max waiting time is bounded: (n−1) × quantum.
→ `include/scheduler.h` → `runRoundRobin()`

**Q: What happens when a process is preempted in RR?**
1. Gantt entry is written for the slice just executed.
2. PCB state → READY, logEvent "PREEMPTED".
3. PCB is pushed to the back of the ready_queue.
The next dequeue picks whoever is now at the front.

**Q: Explain SJF.**
Shortest Job First selects the process with the smallest remaining_time from a min-heap.
It is non-preemptive here — once a process starts it runs to completion.
SJF gives the provably optimal (minimum) average waiting time among non-preemptive algorithms.
Drawback: requires knowing burst time in advance (not always realistic).
→ `include/scheduler.h` → `runSJF()`, `SJFComparator`

**Q: Explain Priority Scheduling.**
A min-heap orders processes by priority value (lower value = higher priority).
Tie-breaking uses arrival time. Non-preemptive in our implementation.
Risk: starvation — low-priority processes may never run if high-priority ones keep arriving.
→ `include/scheduler.h` → `runPriority()`, `PriorityComparator`

**Q: How is starvation detected?**
`checkStarvation()` iterates over READY processes and compares
`current_time − last_scheduled_time` against `STARVATION_THRESHOLD` (20 units).
If exceeded and not yet flagged, `logStarvation()` writes a warning into the PCB log.
→ `include/scheduler.h` → `checkStarvation()`; called in `runPriority()` every 5 ticks.

**Q: Which algorithm gives the best average waiting time?**
SJF — it is mathematically proven to minimise average waiting time for non-preemptive
scheduling when burst times are known. The `AnalyticsEngine` will report this automatically
in the comparative table.

---

## SECTION 4: runRoundRobinIO — Extended Algorithm

**Q: What extra features does runRoundRobinIO() add over plain RR?**
Three extensions:
  1. I/O Waits — processes may block mid-quantum for DISK_READ or NET_RECV.
  2. Mutex Synchronisation — every process must hold SimMutex before executing.
  3. TCB Ticks — tickThreads() keeps the main TCB's remaining_time in sync.
→ FILE: `src/scheduler_io.cpp`

**Q: How does I/O work in the simulation?**
At tick 0 of each quantum, `shouldIssueIO()` uses a deterministic hash
(pid×37 + clock×13) % 100 to decide (30% probability). If triggered:
  - A partial Gantt slice is recorded.
  - The mutex is released immediately (process shouldn't hold lock while sleeping).
  - The main TCB transitions to WAITING.
  - An IORequest is pushed to `io_pending` with a finish tick.
  - On future ticks, `tickIOCompletions()` checks if clock >= finish_tick and wakes the process.
→ `src/scheduler_io.cpp` — step (8), I/O check block

**Q: Why release the mutex before issuing I/O?**
Holding a lock while blocked on I/O would cause all other processes waiting for that mutex
to starve for the entire I/O duration. Releasing it first allows another process to enter
the critical section immediately — this is correct mutex discipline.

**Q: How does the mutex gate work?**
`tryAcquireMutex()` calls `SimMutex::acquire()`. If the lock is free it is granted and
execution continues. If held, the process moves to WAITING, its PID is enqueued in the
mutex waiter queue, and the scheduler records 1 IDLE tick and moves on.
When the owner calls `releaseMutex()`, the next PID is dequeued and its PCB is pushed
back onto the ready queue as READY.
→ `include/scheduler.h` → `tryAcquireMutex()`, `releaseMutex()`
→ `include/sync.h` → `SimMutex::acquire()`, `SimMutex::release()`

---

## SECTION 5: Memory Management

**Q: What is demand paging?**
Pages are not loaded into RAM at process creation. They are loaded only when first accessed.
This saves memory — pages that are never touched are never allocated.
→ `include/memory_manager.h` → `accessMemory()` → `handlePageFault()`

**Q: Walk me through a page fault.**
1. `accessMemory(pcb, logical_page, time, all_pcbs)` is called.
2. `pcb->page_table[logical_page]` is -1 → page fault.
3. `handlePageFault()` is called:
   a. `findFreeFrame()` scans `frame_bitmap` for a 0 bit.
   b. If no free frame → `evictFIFO()` pops the front of `fifo_queue` (oldest frame).
      The evicted frame's owner PCB has its page_table entry reset to -1.
   c. The new frame is marked in `frame_bitmap`, recorded in `frame_owner`,
      pushed to back of `fifo_queue`, and written into `pcb->page_table[logical_page]`.
4. `pcb->logPageFault()` logs the event and increments `page_fault_count`.
→ `include/memory_manager.h`

**Q: What is FIFO page replacement and what is its weakness?**
FIFO evicts the page that has been in memory the longest (front of the deque).
It is simple but suffers from Belady's Anomaly — adding more frames can sometimes
increase the number of page faults (counterintuitive behaviour).

**Q: When is process memory released?**
`releaseProcessMemory()` is called from `finalizeProcess()` when a process terminates.
It iterates the page table, clears every occupied frame from `frame_bitmap`,
removes it from `frame_owner`, and erases it from `fifo_queue`.
→ `include/memory_manager.h` → `releaseProcessMemory()`

---

## SECTION 6: Synchronisation

**Q: What is the difference between a mutex and a semaphore?**
A mutex (SimMutex) is a binary lock — only 1 holder at a time, and only the owner can
release it. A semaphore (SimSemaphore) is a counting primitive — up to N holders
simultaneously, and any process can signal it. Use mutex for mutual exclusion,
semaphore for resource counting or producer-consumer coordination.
→ `include/sync.h`

**Q: What happens when two processes try to acquire the same mutex?**
The second process calls `acquire()`, finds `locked == true`, logs a BLOCKED event,
and returns false. The caller (`tryAcquireMutex`) moves the PCB to WAITING and calls
`enqueueWaiter(pid)`. When the owner calls `release()`, the first PID in the wait queue
is dequeued, ownership is transferred, and that PCB is pushed back onto the ready queue.
→ `include/sync.h` → `SimMutex::acquire()` + `release()`

**Q: How is deadlock avoided in this simulation?**
By ensuring every process acquires only one resource (the shared mutex) and never holds
it while waiting for another resource. The I/O code explicitly releases the mutex before
blocking — so no circular wait can form. Classic deadlock requires: mutual exclusion,
hold-and-wait, no preemption, and circular wait — we break hold-and-wait.

---

## SECTION 7: Gantt Chart & Analytics

**Q: How does the Gantt chart work?**
`addEntry(label, start, end)` appends a GanttEntry. Adjacent entries with the same label
are merged automatically. `render()` draws a boxed ASCII chart, wrapping at 36 time units
per row, with a label row, bar row (=== or ---), and time row.
→ `include/gantt_chart.h`

**Q: How is CPU utilisation calculated?**
`cpu_utilisation = (total_time − idle_time) / total_time × 100`
`idle_time` is the sum of all "IDLE" entries in the Gantt chart.
→ `include/analytics.h` → `computeAndPrint()`

**Q: What does the comparative table show?**
After all four algorithms run on the same workload, `printComparativeTable()` lists
Avg WT, Avg TAT, CPU Util%, Throughput, and Page Faults side-by-side and highlights
the best algorithm for each metric using `std::min_element` / `std::max_element`.
→ `include/analytics.h` → `printComparativeTable()`

---

## SECTION 8: Code Design Questions

**Q: Why are all headers in `include/` and only two `.cpp` files in `src/`?**
Most classes are small enough to be fully header-only (definition + implementation in
one file), which is idiomatic for template-heavy or tightly-coupled C++ code.
`scheduler_io.cpp` is separated because `runRoundRobinIO()` is large and complex —
giving it its own translation unit makes it easier to navigate, compile faster
in isolation, and discuss independently in a viva.

**Q: Why use `std::shared_ptr` for PCB and TCB?**
Multiple parts of the system (Scheduler, MemoryManager, sync maps) need to reference
the same PCB object. `shared_ptr` provides automatic lifetime management — no manual
`delete`, no dangling pointers. The last `shared_ptr` to a PCB drops it automatically
when all algorithms finish.

**Q: What is the purpose of `prepareProcesses()`?**
It is called at the start of every scheduling algorithm. It resets the MemoryManager,
calls `pcb->reset()` on every process (clearing all metrics and rebuilding TCBs),
sorts processes by arrival time, and prints a header banner. This ensures each
algorithm starts from an identical clean slate.
→ `include/scheduler.h` → `prepareProcesses()` (private)

**Q: What does `simulateMemoryAccess()` do each tick?**
It accesses up to 2 logical pages (pages 0 and 1) of the running process. If those
pages are not in RAM, page faults are triggered. This simulates realistic memory
traffic — a process that runs longer will accumulate more page faults.
→ `include/scheduler.h` → `simulateMemoryAccess()` (private)

---

## QUICK FORMULA SHEET (for the board)

```
Waiting Time (WT)       = Completion − Arrival − Burst
Turnaround Time (TAT)   = Completion − Arrival
Response Time           = First_Dispatch − Arrival
CPU Utilisation (%)     = (Total_Time − Idle_Time) / Total_Time × 100
Throughput              = Number_of_Processes / Total_Time

RR max wait bound       = (n − 1) × Quantum
SJF: optimal avg WT     = provably minimum for non-preemptive
```

---

## COMMON TRICK QUESTIONS

**"Is your SJF preemptive?"**
No — once a process starts it runs to completion. Preemptive SJF is called
SRTF (Shortest Remaining Time First) and would require re-evaluating the heap
after every tick when a new process arrives.

**"Can your Priority scheduler cause starvation?"**
Yes — a low-priority process can wait indefinitely if high-priority processes
keep arriving. Our code detects this at threshold=20 units and logs a warning,
but does not implement aging (incrementing priority over time) as a fix.

**"What if two processes have the same burst in SJF?"**
The `SJFComparator` breaks ties by `arrival_time` — the earlier-arriving process
wins. This makes the simulation deterministic and reproducible.

**"What is Belady's Anomaly?"**
With FIFO replacement, increasing the number of physical frames can sometimes
increase page faults. It does not affect LRU or OPT. Our simulator uses FIFO,
so it is theoretically susceptible — though the workloads are too small to trigger it.
