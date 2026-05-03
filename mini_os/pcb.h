#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <iostream>
#include "enums.h"
#include "config.h"
#include "tcb.h"

// ============================================================
// pcb.h  —  Process Control Block
//
//  The PCB is the OS's complete bookkeeping record for one
//  running process.  It stores both scheduling metadata and
//  the thread roster (tcb_list).
//
//  KEY FIELDS (good for viva):
//    pid              — unique process ID
//    state            — ProcessState enum
//    arrival_time     — when the process enters the system
//    burst_time       — total CPU time required (immutable)
//    remaining_time   — decremented each tick; 0 => DONE
//    priority         — lower value = higher priority
//    num_pages        — how many virtual pages this process needs
//    page_table       — maps logical page -> physical frame (-1 = not loaded)
//    page_fault_count — incremented each time a page is not in RAM
//    response_time    — (first_run - arrival); -1 until dispatched
//    last_scheduled_time — used for starvation detection
//    tcb_list         — all threads spawned by this process
// ============================================================

class PCB
{
public:
    // --- Identity & Scheduling ---
    int         pid;
    std::string name;
    ProcessState state;
    int  arrival_time;
    int  burst_time;
    int  remaining_time;
    int  completion_time;
    int  waiting_time;
    int  turnaround_time;
    int  response_time;
    int  priority;

    // --- Memory ---
    int  num_pages;
    std::map<int,int> page_table;     // logical page -> frame (-1 = unmapped)
    int  page_fault_count;

    // --- Miscellaneous ---
    std::vector<std::string> log_history;
    int  last_scheduled_time;
    bool starvation_flagged;

    // --- Thread roster ---
    std::vector<std::shared_ptr<TCB>> tcb_list;
    int next_tid_counter;

    // ----------------------------------------------------------
    PCB(int pid_, const std::string& name_, int arrival_,
        int burst_, int priority_, int num_pages_)
        : pid(pid_), name(name_), state(ProcessState::NEW),
          arrival_time(arrival_), burst_time(burst_),
          remaining_time(burst_), completion_time(0),
          waiting_time(0), turnaround_time(0), response_time(-1),
          priority(priority_), num_pages(num_pages_),
          page_fault_count(0),
          last_scheduled_time(arrival_),
          starvation_flagged(false),
          next_tid_counter(0)
    {
        for (int page = 0; page < num_pages_; ++page)
            page_table[page] = -1;

        logEvent(arrival_time,
            "Process CREATED | Burst=" + std::to_string(burst_time) +
            " | Priority=" + std::to_string(priority_) +
            " | Pages=" + std::to_string(num_pages_));

        // Auto-create the main thread that carries the full burst
        addThread(arrival_time, burst_time);
    }

    // ----------------------------------------------------------
    // Spawn a new thread belonging to this process.
    TCB* addThread(int arrival, int burst_for_thread)
    {
        int tid = (pid * 100) + next_tid_counter++;
        std::string tname = name + "-T" + std::to_string(next_tid_counter - 1);
        auto tcb = std::make_shared<TCB>(tid, pid, tname,
                                         arrival, burst_for_thread);
        tcb_list.push_back(tcb);
        logEvent(arrival,
            "Thread SPAWNED: " + tname +
            " (TID=" + std::to_string(tid) +
            ") Burst=" + std::to_string(burst_for_thread));
        return tcb.get();
    }

    // ----------------------------------------------------------
    bool allThreadsTerminated() const
    {
        return std::all_of(tcb_list.begin(), tcb_list.end(),
            [](const std::shared_ptr<TCB>& t){
                return t->state == ThreadState::TERMINATED;
            });
    }

    // ----------------------------------------------------------
    void printThreadSummary() const
    {
        std::string border(OSConfig::TABLE_WIDTH, '-');
        std::cout << border << "\n";
        std::cout << "  Thread Summary for " << name
                  << " (PID=" << pid << ")  --  "
                  << tcb_list.size() << " thread(s)\n";
        std::cout << border << "\n";
        std::cout << std::left
                  << std::setw(8)  << "TID"
                  << std::setw(14) << "Name"
                  << std::setw(10) << "Arrival"
                  << std::setw(10) << "Burst"
                  << std::setw(12) << "Completion"
                  << std::setw(12) << "TAT"
                  << std::setw(10) << "WT"
                  << "\n";
        std::cout << border << "\n";
        for (const auto& tcb : tcb_list)
        {
            std::cout << std::left
                      << std::setw(8)  << tcb->tid
                      << std::setw(14) << tcb->name
                      << std::setw(10) << tcb->arrival_time
                      << std::setw(10) << tcb->burst_time
                      << std::setw(12) << tcb->completion_time
                      << std::setw(12) << tcb->turnaround_time
                      << std::setw(10) << tcb->waiting_time
                      << "\n";
        }
        std::cout << border << "\n";
    }

    // ----------------------------------------------------------
    void setState(ProcessState new_state, int current_time)
    {
        std::string old_str = stateToString(state);
        state = new_state;
        logEvent(current_time,
            "STATE: " + old_str + " ---> " + stateToString(new_state));
    }

    // ----------------------------------------------------------
    void logEvent(int time_unit, const std::string& event)
    {
        std::ostringstream oss;
        oss << "[T=" << std::setw(4) << std::setfill('0') << time_unit
            << "] [" << name << "] " << event;
        log_history.push_back(oss.str());
    }

    // ----------------------------------------------------------
    void logPageFault(int time_unit, int logical_page,
                      int allocated_frame, int evicted_page = -1)
    {
        ++page_fault_count;
        std::ostringstream oss;
        oss << "PAGE FAULT on logical page " << logical_page
            << " -> Allocated frame " << allocated_frame;
        if (evicted_page >= 0)
            oss << " [FIFO evicted page " << evicted_page << "]";
        logEvent(time_unit, oss.str());
    }

    // ----------------------------------------------------------
    void logStarvation(int time_unit, int wait_duration)
    {
        if (!starvation_flagged)
        {
            starvation_flagged = true;
            logEvent(time_unit,
                "!! STARVATION WARNING: Has waited " +
                std::to_string(wait_duration) + " time units!");
        }
    }

    // ----------------------------------------------------------
    void calculateMetrics(int comp_time)
    {
        completion_time  = comp_time;
        turnaround_time  = completion_time - arrival_time;
        waiting_time     = std::max(0, turnaround_time - burst_time);
        logEvent(comp_time,
            "COMPLETED | TAT=" + std::to_string(turnaround_time) +
            " | WT=" + std::to_string(waiting_time) +
            " | Page Faults=" + std::to_string(page_fault_count));
    }

    // ----------------------------------------------------------
    void printLog() const
    {
        std::string border(OSConfig::TABLE_WIDTH, '-');
        std::cout << border << "\n";
        std::cout << "  PCB Internal Log Dump for Process: "
                  << name << " (PID=" << pid << ")\n";
        std::cout << border << "\n";
        for (const auto& entry : log_history)
            std::cout << "  " << entry << "\n";
        std::cout << border << "\n";
    }

    // ----------------------------------------------------------
    // Full reset — called before re-running a scheduler algorithm.
    void reset()
    {
        state               = ProcessState::NEW;
        remaining_time      = burst_time;
        completion_time     = 0;
        waiting_time        = 0;
        turnaround_time     = 0;
        response_time       = -1;
        page_fault_count    = 0;
        last_scheduled_time = arrival_time;
        starvation_flagged  = false;
        log_history.clear();

        for (auto& entry : page_table)
            entry.second = -1;

        logEvent(arrival_time,
            "Process RESET for new scheduling run | Burst=" +
            std::to_string(burst_time) +
            " | Priority=" + std::to_string(priority));

        // Rebuild thread roster from scratch
        next_tid_counter = 0;
        tcb_list.clear();
        addThread(arrival_time, burst_time);
    }
};
