#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include "enums.h"
#include "config.h"

// ============================================================
// tcb.h  —  Thread Control Block
//
//  Each PCB owns one or more TCBs.
//  The "main" TCB is auto-created in PCB::PCB() with the full
//  burst of its parent process.  Additional worker threads can
//  be added via PCB::addThread().
//
//  KEY FIELDS (good for viva):
//    tid              — unique thread ID  (pid*100 + local idx)
//    owner_pid        — parent process ID
//    state            — ThreadState enum (NEW/READY/RUNNING/WAITING/TERMINATED)
//    burst_time       — total CPU needed
//    remaining_time   — decremented each tick inside tickThreads()
//    waiting_time     — computed at completion  (TAT - burst)
//    turnaround_time  — completion_time - arrival_time
//    response_time    — first dispatch - arrival  (-1 until first run)
//    io_wait_remaining— ticks left for a pending I/O (unused in base RR)
//    blocked_on_resource — name of mutex/semaphore blocking this thread
// ============================================================

struct TCB
{
    // Identity
    int         tid;
    int         owner_pid;
    std::string name;

    // Scheduling state
    ThreadState state;
    int arrival_time;
    int burst_time;
    int remaining_time;
    int waiting_time;
    int turnaround_time;
    int completion_time;
    int response_time;

    // Synchronisation / I/O bookkeeping
    std::string blocked_on_resource;
    int         io_wait_remaining;

    // Execution history
    std::vector<std::string> log_history;

    // ----------------------------------------------------------
    TCB(int tid_, int owner_pid_, const std::string& tname,
        int arrival_, int burst_)
        : tid(tid_), owner_pid(owner_pid_), name(tname),
          state(ThreadState::NEW),
          arrival_time(arrival_), burst_time(burst_),
          remaining_time(burst_),
          waiting_time(0), turnaround_time(0),
          completion_time(0), response_time(-1),
          io_wait_remaining(0)
    {
        logEvent(arrival_,
            "Thread CREATED | Burst=" + std::to_string(burst_) +
            " | Owner PID=" + std::to_string(owner_pid_));
    }

    // ----------------------------------------------------------
    void setState(ThreadState new_state, int current_time)
    {
        std::string old_str = threadStateToString(state);
        state = new_state;
        logEvent(current_time,
            "THREAD STATE: " + old_str +
            " ---> " + threadStateToString(new_state));
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
    // Called when remaining_time reaches 0.
    void calculateMetrics(int comp_time)
    {
        completion_time  = comp_time;
        turnaround_time  = completion_time - arrival_time;
        waiting_time     = std::max(0, turnaround_time - burst_time);
        logEvent(comp_time,
            "Thread COMPLETE | TAT=" + std::to_string(turnaround_time) +
            " | WT=" + std::to_string(waiting_time));
    }

    // ----------------------------------------------------------
    void printLog() const
    {
        std::string border(OSConfig::TABLE_WIDTH, '-');
        std::cout << border << "\n";
        std::cout << "  TCB Log: " << name
                  << " (TID=" << tid
                  << ", owner PID=" << owner_pid << ")\n";
        std::cout << border << "\n";
        for (const auto& entry : log_history)
            std::cout << "  " << entry << "\n";
        std::cout << border << "\n";
    }

    // ----------------------------------------------------------
    // Resets all fields — used before re-running a scheduler.
    void reset()
    {
        state             = ThreadState::NEW;
        remaining_time    = burst_time;
        waiting_time      = 0;
        turnaround_time   = 0;
        completion_time   = 0;
        response_time     = -1;
        io_wait_remaining = 0;
        blocked_on_resource.clear();
        log_history.clear();
        logEvent(arrival_time,
            "Thread RESET | Burst=" + std::to_string(burst_time));
    }
};
