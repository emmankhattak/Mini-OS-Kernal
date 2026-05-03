// ============================================================
// scheduler_io.cpp  —  runRoundRobinIO() implementation
//
//  Separated from scheduler.h because it is the most complex
//  algorithm and benefits from its own compilation unit and
//  focused viva discussion.
//
//  Flow summary (good for viva):
//    1. Reset sync (mutex, semaphore) and I/O tables.
//    2. Admit arriving processes to ready_queue.
//    3. Tick-loop:
//         a. Wake processes whose I/O timer expired.
//         b. Admit newly arrived processes.
//         c. If ready_queue empty → find next event, emit IDLE.
//         d. Dequeue candidate; try acquiring the shared mutex.
//            If blocked → IDLE for 1 tick, continue.
//         e. Run process for up to `quantum` ticks:
//              - simulateMemoryAccess (page faults)
//              - tickThreads (decrement TCB remaining_time)
//              - On first tick: stochastic I/O check (30%).
//                If I/O issued → emit partial Gantt, release
//                mutex, block TCB, continue outer loop.
//         f. After quantum:
//              - remaining == 0 → finalize, release mutex.
//              - else           → preempt, re-enqueue, release mutex.
// ============================================================

#include "scheduler.h"
#include <iostream>
#include <climits>
#include <algorithm>

GanttChart Scheduler::runRoundRobinIO(int quantum)
{
    // ---- Reset sync primitives and I/O bookkeeping ----------
    shared_mutex      = std::make_shared<SimMutex>("SharedResource");
    shared_semaphore  = std::make_shared<SimSemaphore>("IOSlots", 2);
    io_pending.clear();
    io_blocked.clear();
    sync_blocked.clear();

    prepareProcesses(SchedulerType::ROUND_ROBIN);

    GanttChart gantt("Round Robin + I/O + Mutex (Q=" +
                     std::to_string(quantum) + ")");

    std::queue<std::shared_ptr<PCB>> ready_queue;

    int clock     = processes.front()->arrival_time;
    int n         = static_cast<int>(processes.size());
    int completed = 0;
    std::vector<bool> admitted(n, false);

    constexpr int IO_PROBABILITY_PCT = 30;
    constexpr int IO_DURATION_MIN    = 3;
    constexpr int IO_DURATION_MAX    = 6;

    // ---- Lambda: admit processes that have arrived ----------
    auto admitArrivals = [&]()
    {
        for (int i = 0; i < n; ++i)
        {
            if (!admitted[i] &&
                processes[i]->arrival_time <= clock &&
                processes[i]->state == ProcessState::NEW)
            {
                processes[i]->setState(ProcessState::READY, clock);
                if (!processes[i]->tcb_list.empty())
                    processes[i]->tcb_list[0]->setState(
                        ThreadState::READY, clock);
                ready_queue.push(processes[i]);
                admitted[i] = true;
            }
        }
    };

    // ---- Lambda: are any processes not yet TERMINATED? ------
    auto anyRemaining = [&]() -> bool
    {
        return std::any_of(processes.begin(), processes.end(),
            [](const std::shared_ptr<PCB>& p)
            { return p->state != ProcessState::TERMINATED; });
    };

    admitArrivals();

    // =========================================================
    // Main simulation loop
    // =========================================================
    while (anyRemaining())
    {
        // (1) Wake processes whose I/O has finished
        {
            auto woken = tickIOCompletions(clock);
            for (auto& pcb : woken)
            {
                pcb->setState(ProcessState::READY, clock);
                if (!pcb->tcb_list.empty() &&
                    pcb->tcb_list[0]->state == ThreadState::WAITING)
                    pcb->tcb_list[0]->setState(ThreadState::READY, clock);
                ready_queue.push(pcb);
            }
        }

        // (2) Admit newly arrived processes
        admitArrivals();

        // (3) CPU idle — find next interesting event
        if (ready_queue.empty())
        {
            int next_event = INT_MAX;
            for (const auto& req : io_pending)
                next_event = std::min(next_event, req.io_finish_tick);
            for (int i = 0; i < n; ++i)
                if (!admitted[i])
                    next_event = std::min(next_event,
                                          processes[i]->arrival_time);
            if (next_event == INT_MAX) break;  // deadlock guard
            gantt.addEntry("IDLE", clock, next_event);
            clock = next_event;
            continue;
        }

        // (4) Dequeue next candidate
        auto current = ready_queue.front();
        ready_queue.pop();

        // (5) Mutex gate — block if lock is already held
        if (!tryAcquireMutex(current, clock))
        {
            gantt.addEntry("IDLE", clock, clock + 1);
            ++clock;
            continue;
        }

        // (6) Record first-dispatch response time
        if (current->response_time == -1)
            current->response_time = clock - current->arrival_time;

        // (7) Transition to RUNNING
        current->setState(ProcessState::RUNNING, clock);
        if (!current->tcb_list.empty())
            current->tcb_list[0]->setState(ThreadState::RUNNING, clock);
        current->last_scheduled_time = clock;

        current->logEvent(clock,
            "DISPATCH | Mutex held | Remaining=" +
            std::to_string(current->remaining_time));

        // (8) Execute one quantum tick-by-tick
        int burst_start    = clock;
        int ticks_run      = 0;
        bool io_interrupted = false;

        for (int tick = 0;
             tick < quantum && current->remaining_time > 0;
             ++tick)
        {
            simulateMemoryAccess(current, clock);
            tickThreads(current, clock);

            --current->remaining_time;
            ++clock;
            ++ticks_run;

            admitArrivals();

            // Stochastic I/O check — fires at most once per quantum
            if (tick == 0 &&
                current->remaining_time > 0 &&
                shouldIssueIO(current, clock, IO_PROBABILITY_PCT))
            {
                int io_dur =
                    IO_DURATION_MIN +
                    ((current->pid * 7 + clock * 3) %
                     (IO_DURATION_MAX - IO_DURATION_MIN + 1));

                std::string io_type =
                    (current->pid % 2 == 0) ? "DISK_READ" : "NET_RECV";

                // Record partial Gantt slice before blocking
                gantt.addEntry(current->name, burst_start, clock);
                burst_start = -1;

                // Release mutex before sleeping on I/O
                releaseMutex(current, clock, ready_queue);

                // Block main TCB
                if (!current->tcb_list.empty())
                    current->tcb_list[0]->setState(
                        ThreadState::WAITING, clock);

                issueIORequest(current, clock, io_dur, io_type);
                io_interrupted = true;
                break;
            }
        }

        // (9) Emit Gantt if not already done by the I/O branch
        if (burst_start != -1)
            gantt.addEntry(current->name, burst_start, clock);

        // (10) Post-quantum decision
        if (io_interrupted)
        {
            // Process is in WAITING — outer loop handles wakeup
            continue;
        }

        if (current->remaining_time == 0)
        {
            releaseMutex(current, clock, ready_queue);
            if (!current->tcb_list.empty())
                current->tcb_list[0]->setState(
                    ThreadState::TERMINATED, clock);
            finalizeProcess(current, clock);
            ++completed;
        }
        else
        {
            // Quantum expired; preempt and re-enqueue
            releaseMutex(current, clock, ready_queue);
            current->setState(ProcessState::READY, clock);
            if (!current->tcb_list.empty())
                current->tcb_list[0]->setState(ThreadState::READY, clock);
            current->logEvent(clock,
                "PREEMPTED after Q=" + std::to_string(ticks_run) +
                " | Remaining=" + std::to_string(current->remaining_time));
            ready_queue.push(current);
        }
    }

    // ---- Output ---------------------------------------------
    gantt.render();
    analytics.computeAndPrint(processes, gantt,
        SchedulerType::ROUND_ROBIN, *memory_manager,
        workload_name + " [+IO+Mutex]");

    std::cout << "\n  THREAD EXECUTION SUMMARY:\n";
    for (const auto& pcb : processes)
        pcb->printThreadSummary();

    shared_mutex->printLog();

    return gantt;
}
