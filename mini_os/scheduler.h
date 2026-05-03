#pragma once
#include <vector>
#include <queue>
#include <memory>
#include <map>
#include <algorithm>
#include <climits>
#include <iostream>
#include <string>
#include "config.h"
#include "enums.h"
#include "pcb.h"
#include "memory_manager.h"
#include "sync.h"
#include "gantt_chart.h"
#include "analytics.h"

// ============================================================
// scheduler.h  —  CPU Scheduling Algorithms
//
//  Four classic algorithms + one extended variant:
//    runFCFS()         — non-preemptive, FIFO queue
//    runRoundRobin()   — preemptive, fixed quantum
//    runSJF()          — non-preemptive, min-heap on remaining_time
//    runPriority()     — non-preemptive, min-heap on priority value
//    runRoundRobinIO() — RR + stochastic I/O + SimMutex + TCB ticks
//    runAllAlgorithms()— runs all four base algorithms then compares
//
//  Internal helpers (private):
//    prepareProcesses()   — reset state, sort by arrival, print header
//    simulateMemoryAccess()— access up to 2 pages per tick (page faults)
//    finalizeProcess()    — TERMINATED state + metrics + free memory
//    checkStarvation()    — warn if a READY process waited too long
//    shouldIssueIO()      — deterministic hash-based I/O trigger
//    issueIORequest()     — block process, register IORequest
//    tickIOCompletions()  — return processes whose I/O is now done
//    tryAcquireMutex()    — gate process through SimMutex
//    releaseMutex()       — unlock + wake next waiter into ready_queue
//    tickThreads()        — decrement running TCB's remaining_time
// ============================================================

class Scheduler
{
private:
    std::vector<std::shared_ptr<PCB>> processes;
    std::shared_ptr<MemoryManager>    memory_manager;
    AnalyticsEngine                   analytics;
    std::string                       workload_name;

    // --- Sync / I/O state (used only by runRoundRobinIO) ---
    std::shared_ptr<SimMutex>      shared_mutex;
    std::shared_ptr<SimSemaphore>  shared_semaphore;
    std::map<int, std::shared_ptr<PCB>> sync_blocked;
    std::vector<IORequest>              io_pending;
    std::map<int, std::shared_ptr<PCB>> io_blocked;

    // ---- Priority queue comparators -------------------------

    struct SJFComparator
    {
        bool operator()(const std::shared_ptr<PCB>& a,
                        const std::shared_ptr<PCB>& b) const
        {
            if (a->remaining_time == b->remaining_time)
                return a->arrival_time > b->arrival_time;
            return a->remaining_time > b->remaining_time;
        }
    };

    struct PriorityComparator
    {
        bool operator()(const std::shared_ptr<PCB>& a,
                        const std::shared_ptr<PCB>& b) const
        {
            if (a->priority == b->priority)
                return a->arrival_time > b->arrival_time;
            return a->priority > b->priority;
        }
    };

    // ---- Shared helpers -------------------------------------

    void prepareProcesses(SchedulerType algo_type)
    {
        memory_manager->reset();
        for (auto& pcb : processes) pcb->reset();

        std::sort(processes.begin(), processes.end(),
            [](const std::shared_ptr<PCB>& a, const std::shared_ptr<PCB>& b)
            { return a->arrival_time < b->arrival_time; });

        std::string div(OSConfig::TABLE_WIDTH, '*');
        std::cout << "\n" << div << "\n";
        std::cout << "  RUNNING: " << schedulerTypeToString(algo_type)
                  << " on Workload: " << workload_name << "\n";
        std::cout << div << "\n";
    }

    void simulateMemoryAccess(std::shared_ptr<PCB>& pcb, int current_time)
    {
        int pages = std::min(pcb->num_pages, 2);
        for (int page = 0; page < pages; ++page)
            memory_manager->accessMemory(pcb, page, current_time, processes);
    }

    void finalizeProcess(std::shared_ptr<PCB>& pcb, int current_time)
    {
        pcb->setState(ProcessState::TERMINATED, current_time);
        pcb->calculateMetrics(current_time);
        memory_manager->releaseProcessMemory(pcb);
    }

    void checkStarvation(const std::vector<std::shared_ptr<PCB>>& watch,
                         int current_time)
    {
        for (const auto& pcb : watch)
            if (pcb->state == ProcessState::READY)
            {
                int waited = current_time - pcb->last_scheduled_time;
                if (waited >= OSConfig::STARVATION_THRESHOLD)
                    pcb->logStarvation(current_time, waited);
            }
    }

    // ---- I/O helpers ----------------------------------------

    bool shouldIssueIO(const std::shared_ptr<PCB>& pcb,
                       int current_time, int io_probability_pct = 25)
    {
        // Deterministic hash → reproducible output across runs
        int seed = (pcb->pid * 37 + current_time * 13) % 100;
        return seed < io_probability_pct;
    }

    void issueIORequest(std::shared_ptr<PCB>& pcb,
                        int current_time, int io_duration,
                        const std::string& io_type)
    {
        pcb->setState(ProcessState::WAITING, current_time);
        pcb->logEvent(current_time,
            "IO_REQUEST: " + io_type +
            " | Duration=" + std::to_string(io_duration) +
            " | Resume at T=" + std::to_string(current_time + io_duration));
        io_pending.emplace_back(pcb->pid, current_time + io_duration, io_type);
        io_blocked[pcb->pid] = pcb;
    }

    std::vector<std::shared_ptr<PCB>> tickIOCompletions(int current_time)
    {
        std::vector<std::shared_ptr<PCB>> woken;
        io_pending.erase(
            std::remove_if(io_pending.begin(), io_pending.end(),
                [&](const IORequest& req) -> bool
                {
                    if (current_time >= req.io_finish_tick)
                    {
                        auto it = io_blocked.find(req.pid);
                        if (it != io_blocked.end())
                        {
                            auto& pcb = it->second;
                            pcb->logEvent(current_time,
                                "IO_COMPLETE: " + req.io_type +
                                " | Returning to READY");
                            woken.push_back(pcb);
                            io_blocked.erase(it);
                        }
                        return true;
                    }
                    return false;
                }),
            io_pending.end());
        return woken;
    }

    // ---- Mutex helpers --------------------------------------

    bool tryAcquireMutex(std::shared_ptr<PCB>& pcb, int current_time)
    {
        if (!shared_mutex) return true;
        bool granted = shared_mutex->acquire(pcb->pid, current_time);
        if (!granted)
        {
            pcb->setState(ProcessState::WAITING, current_time);
            pcb->logEvent(current_time,
                "SYNC_BLOCK: Waiting for mutex '" +
                shared_mutex->resource_name + "'");
            shared_mutex->enqueueWaiter(pcb->pid);
            sync_blocked[pcb->pid] = pcb;
        }
        return granted;
    }

    void releaseMutex(std::shared_ptr<PCB>& pcb, int current_time,
                      std::queue<std::shared_ptr<PCB>>& ready_queue)
    {
        if (!shared_mutex) return;
        int next_pid = shared_mutex->release(pcb->pid, current_time);
        if (next_pid != -1)
        {
            auto it = sync_blocked.find(next_pid);
            if (it != sync_blocked.end())
            {
                auto& next_pcb = it->second;
                next_pcb->setState(ProcessState::READY, current_time);
                next_pcb->logEvent(current_time,
                    "SYNC_UNBLOCK: Acquired mutex '" +
                    shared_mutex->resource_name + "'");
                ready_queue.push(next_pcb);
                sync_blocked.erase(it);
            }
        }
    }

    // ---- Thread tick ----------------------------------------

    void tickThreads(std::shared_ptr<PCB>& pcb, int current_time)
    {
        for (auto& tcb : pcb->tcb_list)
        {
            if (tcb->state == ThreadState::RUNNING)
            {
                if (tcb->remaining_time > 0)
                    --tcb->remaining_time;
                if (tcb->remaining_time == 0)
                {
                    tcb->calculateMetrics(current_time + 1);
                    tcb->setState(ThreadState::TERMINATED, current_time + 1);
                }
            }
        }
    }

public:
    // ----------------------------------------------------------
    Scheduler(std::vector<std::shared_ptr<PCB>> procs,
              std::shared_ptr<MemoryManager>    mem_mgr,
              const std::string&                wkld_name)
        : processes(std::move(procs)),
          memory_manager(std::move(mem_mgr)),
          workload_name(wkld_name)
    {}

    // ----------------------------------------------------------
    // 1. FCFS — First Come First Serve (non-preemptive)
    // ----------------------------------------------------------
    GanttChart runFCFS()
    {
        prepareProcesses(SchedulerType::FCFS);
        GanttChart gantt(schedulerTypeToString(SchedulerType::FCFS));

        std::queue<std::shared_ptr<PCB>> ready_queue;
        int clock = processes.front()->arrival_time;
        int n     = static_cast<int>(processes.size());
        int completed = 0;

        while (completed < n)
        {
            for (auto& pcb : processes)
                if (pcb->arrival_time <= clock && pcb->state == ProcessState::NEW)
                {
                    pcb->setState(ProcessState::READY, clock);
                    ready_queue.push(pcb);
                }

            if (ready_queue.empty())
            {
                gantt.addEntry("IDLE", clock, clock + 1);
                ++clock; continue;
            }

            auto current = ready_queue.front(); ready_queue.pop();
            if (current->response_time == -1)
                current->response_time = clock - current->arrival_time;

            current->setState(ProcessState::RUNNING, clock);
            current->last_scheduled_time = clock;
            int burst_start = clock;

            while (current->remaining_time > 0)
            {
                simulateMemoryAccess(current, clock);
                --current->remaining_time;
                ++clock;
                for (auto& pcb : processes)
                    if (pcb->arrival_time == clock && pcb->state == ProcessState::NEW)
                    {
                        pcb->setState(ProcessState::READY, clock);
                        ready_queue.push(pcb);
                    }
            }

            gantt.addEntry(current->name, burst_start, clock);
            finalizeProcess(current, clock);
            ++completed;
        }

        gantt.render();
        analytics.computeAndPrint(processes, gantt,
            SchedulerType::FCFS, *memory_manager, workload_name);
        return gantt;
    }

    // ----------------------------------------------------------
    // 2. Round Robin (preemptive, quantum = RR_TIME_QUANTUM)
    // ----------------------------------------------------------
    GanttChart runRoundRobin(int quantum = OSConfig::RR_TIME_QUANTUM)
    {
        prepareProcesses(SchedulerType::ROUND_ROBIN);
        GanttChart gantt(schedulerTypeToString(SchedulerType::ROUND_ROBIN));

        std::queue<std::shared_ptr<PCB>> ready_queue;
        int clock = processes.front()->arrival_time;
        int n     = static_cast<int>(processes.size());
        int completed = 0;
        std::vector<bool> admitted(n, false);

        for (int i = 0; i < n; ++i)
            if (processes[i]->arrival_time <= clock &&
                processes[i]->state == ProcessState::NEW)
            {
                processes[i]->setState(ProcessState::READY, clock);
                ready_queue.push(processes[i]);
                admitted[i] = true;
            }

        while (completed < n)
        {
            for (int i = 0; i < n; ++i)
                if (!admitted[i] &&
                    processes[i]->arrival_time <= clock &&
                    processes[i]->state == ProcessState::NEW)
                {
                    processes[i]->setState(ProcessState::READY, clock);
                    ready_queue.push(processes[i]);
                    admitted[i] = true;
                }

            if (ready_queue.empty())
            {
                int next = INT_MAX;
                for (int i = 0; i < n; ++i)
                    if (!admitted[i])
                        next = std::min(next, processes[i]->arrival_time);
                if (next == INT_MAX) break;
                gantt.addEntry("IDLE", clock, next);
                clock = next; continue;
            }

            auto current = ready_queue.front(); ready_queue.pop();
            if (current->response_time == -1)
                current->response_time = clock - current->arrival_time;

            current->setState(ProcessState::RUNNING, clock);
            current->last_scheduled_time = clock;
            int burst_start  = clock;
            int time_to_run  = std::min(quantum, current->remaining_time);

            for (int tick = 0; tick < time_to_run; ++tick)
            {
                simulateMemoryAccess(current, clock);
                --current->remaining_time;
                ++clock;
                for (int i = 0; i < n; ++i)
                    if (!admitted[i] &&
                        processes[i]->arrival_time <= clock &&
                        processes[i]->state == ProcessState::NEW)
                    {
                        processes[i]->setState(ProcessState::READY, clock);
                        ready_queue.push(processes[i]);
                        admitted[i] = true;
                    }
            }

            gantt.addEntry(current->name, burst_start, clock);

            if (current->remaining_time == 0)
            {
                finalizeProcess(current, clock);
                ++completed;
            }
            else
            {
                current->setState(ProcessState::READY, clock);
                current->logEvent(clock,
                    "PREEMPTED after quantum=" + std::to_string(quantum) +
                    " | Remaining=" + std::to_string(current->remaining_time));
                ready_queue.push(current);
            }
        }

        gantt.render();
        analytics.computeAndPrint(processes, gantt,
            SchedulerType::ROUND_ROBIN, *memory_manager, workload_name);
        return gantt;
    }

    // ----------------------------------------------------------
    // 3. SJF — Shortest Job First (non-preemptive, min-heap)
    // ----------------------------------------------------------
    GanttChart runSJF()
    {
        prepareProcesses(SchedulerType::SJF);
        GanttChart gantt(schedulerTypeToString(SchedulerType::SJF));

        std::priority_queue<std::shared_ptr<PCB>,
            std::vector<std::shared_ptr<PCB>>, SJFComparator> heap;

        int clock     = processes.front()->arrival_time;
        int n         = static_cast<int>(processes.size());
        int completed = 0;

        auto admit = [&]()
        {
            for (auto& pcb : processes)
                if (pcb->arrival_time <= clock && pcb->state == ProcessState::NEW)
                {
                    pcb->setState(ProcessState::READY, clock);
                    heap.push(pcb);
                }
        };
        admit();

        while (completed < n)
        {
            admit();
            if (heap.empty())
            {
                int next = INT_MAX;
                for (const auto& pcb : processes)
                    if (pcb->state == ProcessState::NEW)
                        next = std::min(next, pcb->arrival_time);
                if (next == INT_MAX) break;
                gantt.addEntry("IDLE", clock, next);
                clock = next; admit(); continue;
            }

            auto current = heap.top(); heap.pop();
            if (current->response_time == -1)
                current->response_time = clock - current->arrival_time;

            current->setState(ProcessState::RUNNING, clock);
            current->last_scheduled_time = clock;
            int burst_start = clock;

            while (current->remaining_time > 0)
            {
                simulateMemoryAccess(current, clock);
                --current->remaining_time;
                ++clock;
                for (auto& pcb : processes)
                    if (pcb->arrival_time == clock && pcb->state == ProcessState::NEW)
                    {
                        pcb->setState(ProcessState::READY, clock);
                        heap.push(pcb);
                    }
            }

            gantt.addEntry(current->name, burst_start, clock);
            finalizeProcess(current, clock);
            ++completed;
        }

        gantt.render();
        analytics.computeAndPrint(processes, gantt,
            SchedulerType::SJF, *memory_manager, workload_name);
        return gantt;
    }

    // ----------------------------------------------------------
    // 4. Priority Scheduling (non-preemptive, min-heap on priority)
    // ----------------------------------------------------------
    GanttChart runPriority()
    {
        prepareProcesses(SchedulerType::PRIORITY);
        GanttChart gantt(schedulerTypeToString(SchedulerType::PRIORITY));

        std::priority_queue<std::shared_ptr<PCB>,
            std::vector<std::shared_ptr<PCB>>, PriorityComparator> heap;

        int clock     = processes.front()->arrival_time;
        int n         = static_cast<int>(processes.size());
        int completed = 0;
        std::vector<std::shared_ptr<PCB>> starvation_watch;

        auto admit = [&]()
        {
            for (auto& pcb : processes)
                if (pcb->arrival_time <= clock && pcb->state == ProcessState::NEW)
                {
                    pcb->setState(ProcessState::READY, clock);
                    heap.push(pcb);
                    starvation_watch.push_back(pcb);
                }
        };
        admit();

        while (completed < n)
        {
            admit();
            checkStarvation(starvation_watch, clock);

            if (heap.empty())
            {
                int next = INT_MAX;
                for (const auto& pcb : processes)
                    if (pcb->state == ProcessState::NEW)
                        next = std::min(next, pcb->arrival_time);
                if (next == INT_MAX) break;
                gantt.addEntry("IDLE", clock, next);
                clock = next; admit(); continue;
            }

            auto current = heap.top(); heap.pop();
            starvation_watch.erase(
                std::remove_if(starvation_watch.begin(), starvation_watch.end(),
                    [&current](const std::shared_ptr<PCB>& p)
                    { return p->pid == current->pid; }),
                starvation_watch.end());

            if (current->response_time == -1)
                current->response_time = clock - current->arrival_time;

            current->setState(ProcessState::RUNNING, clock);
            current->last_scheduled_time = clock;
            current->logEvent(clock,
                "Selected with Priority=" + std::to_string(current->priority) +
                " from " + std::to_string(heap.size() + 1) + " ready processes");

            int burst_start = clock;
            while (current->remaining_time > 0)
            {
                simulateMemoryAccess(current, clock);
                --current->remaining_time;
                ++clock;
                for (auto& pcb : processes)
                    if (pcb->arrival_time == clock && pcb->state == ProcessState::NEW)
                    {
                        pcb->setState(ProcessState::READY, clock);
                        heap.push(pcb);
                        starvation_watch.push_back(pcb);
                    }
                if (clock % 5 == 0)
                    checkStarvation(starvation_watch, clock);
            }

            gantt.addEntry(current->name, burst_start, clock);
            finalizeProcess(current, clock);
            ++completed;
        }

        gantt.render();
        analytics.computeAndPrint(processes, gantt,
            SchedulerType::PRIORITY, *memory_manager, workload_name);
        return gantt;
    }

    // ----------------------------------------------------------
    // 5. Round Robin + I/O Waits + Mutex + TCB Ticks
    // ----------------------------------------------------------
    GanttChart runRoundRobinIO(int quantum = OSConfig::RR_TIME_QUANTUM);

    // ----------------------------------------------------------
    // 6. Run all four base algorithms then print comparative table
    // ----------------------------------------------------------
    void runAllAlgorithms(int dump_log_pid = -1)
    {
        analytics.clearResults();
        runFCFS();
        runRoundRobin();
        runSJF();
        runPriority();

        analytics.printComparativeTable(workload_name);

        std::string header(OSConfig::TABLE_WIDTH, '~');
        std::cout << "\n" << header << "\n";
        std::cout << "  PCB INTERNAL LOG DEMONSTRATION\n";
        std::cout << "  (Showing log from PRIORITY SCHEDULING run)\n";
        std::cout << header << "\n";

        std::shared_ptr<PCB> log_target;
        for (const auto& pcb : processes)
            if (dump_log_pid == -1 || pcb->pid == dump_log_pid)
            {
                log_target = pcb; break;
            }
        if (log_target) log_target->printLog();
    }

    const std::vector<std::shared_ptr<PCB>>& getProcesses() const
    {
        return processes;
    }
};
