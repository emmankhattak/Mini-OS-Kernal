#pragma once
#include <string>
#include <queue>
#include <vector>
#include <sstream>
#include <iomanip>
#include <iostream>
#include "config.h"

// ============================================================
// sync.h  —  SimMutex (Binary Lock) + SimSemaphore (Counting)
//
//  SimMutex — binary mutex:
//    acquire(pid) : returns true if lock was free (granted).
//                   returns false if already held (caller blocks).
//    release(pid) : unlocks; wakes next waiter in FIFO queue.
//    enqueueWaiter(pid): called externally to add blocked PID.
//
//  SimSemaphore — counting semaphore:
//    wait_op(pid)   : P() — decrement count; block if count==0.
//    signal_op(pid) : V() — wake a waiter or increment count.
//
//  Both maintain an internal log for viva demonstration.
// ============================================================

// ============================================================
// SimMutex
// ============================================================
class SimMutex
{
public:
    std::string resource_name;

private:
    bool  locked;
    int   owner_pid;
    std::queue<int>          wait_queue;
    std::vector<std::string> log;

    void logEvent(int time, const std::string& msg)
    {
        std::ostringstream oss;
        oss << "[T=" << std::setw(4) << std::setfill('0') << time
            << "] [MUTEX:" << resource_name << "] " << msg;
        log.push_back(oss.str());
    }

public:
    explicit SimMutex(const std::string& name)
        : resource_name(name), locked(false), owner_pid(-1) {}

    // Returns true => lock granted immediately.
    // Returns false => caller must transition to WAITING and call enqueueWaiter.
    bool acquire(int requesting_pid, int current_time)
    {
        if (!locked)
        {
            locked    = true;
            owner_pid = requesting_pid;
            logEvent(current_time,
                "ACQUIRED by PID=" + std::to_string(requesting_pid));
            return true;
        }
        logEvent(current_time,
            "BLOCKED PID=" + std::to_string(requesting_pid) +
            " -- held by PID=" + std::to_string(owner_pid));
        return false;
    }

    void enqueueWaiter(int pid) { wait_queue.push(pid); }

    // Returns the next PID to wake, or -1 if queue is empty.
    int release(int releasing_pid, int current_time)
    {
        if (owner_pid != releasing_pid)
        {
            logEvent(current_time,
                "ERROR: release() by non-owner PID=" +
                std::to_string(releasing_pid));
            return -1;
        }
        logEvent(current_time,
            "RELEASED by PID=" + std::to_string(releasing_pid));

        if (!wait_queue.empty())
        {
            int next_pid = wait_queue.front();
            wait_queue.pop();
            owner_pid = next_pid;
            logEvent(current_time,
                "OWNERSHIP transferred to PID=" +
                std::to_string(next_pid));
            return next_pid;
        }
        locked    = false;
        owner_pid = -1;
        return -1;
    }

    bool isLocked()   const { return locked; }
    int  getOwner()   const { return owner_pid; }
    int  waitCount()  const { return static_cast<int>(wait_queue.size()); }

    void printLog() const
    {
        std::string border(OSConfig::TABLE_WIDTH, '-');
        std::cout << border << "\n";
        std::cout << "  Mutex Log: " << resource_name << "\n";
        std::cout << border << "\n";
        for (const auto& entry : log)
            std::cout << "  " << entry << "\n";
        std::cout << border << "\n";
    }

    void reset()
    {
        locked    = false;
        owner_pid = -1;
        while (!wait_queue.empty()) wait_queue.pop();
        log.clear();
    }
};


// ============================================================
// SimSemaphore
// ============================================================
class SimSemaphore
{
public:
    std::string resource_name;

private:
    int count;
    int max_count;
    std::queue<int>          wait_queue;
    std::vector<std::string> log;

    void logEvent(int time, const std::string& msg)
    {
        std::ostringstream oss;
        oss << "[T=" << std::setw(4) << std::setfill('0') << time
            << "] [SEM:" << resource_name << "] " << msg;
        log.push_back(oss.str());
    }

public:
    explicit SimSemaphore(const std::string& name, int initial_count = 1)
        : resource_name(name), count(initial_count), max_count(initial_count) {}

    // P() / down() — returns true if permit granted immediately.
    bool wait_op(int requesting_pid, int current_time)
    {
        if (count > 0)
        {
            --count;
            logEvent(current_time,
                "WAIT granted PID=" + std::to_string(requesting_pid) +
                " | Remaining permits=" + std::to_string(count));
            return true;
        }
        logEvent(current_time,
            "WAIT blocked PID=" + std::to_string(requesting_pid) +
            " | count=0");
        return false;
    }

    void enqueueWaiter(int pid) { wait_queue.push(pid); }

    // V() / up() — returns PID to wake, or -1.
    int signal_op(int releasing_pid, int current_time)
    {
        logEvent(current_time,
            "SIGNAL by PID=" + std::to_string(releasing_pid));
        if (!wait_queue.empty())
        {
            int next_pid = wait_queue.front();
            wait_queue.pop();
            logEvent(current_time,
                "Permit transferred to PID=" + std::to_string(next_pid));
            return next_pid;
        }
        if (count < max_count)
            ++count;
        return -1;
    }

    int getCount()   const { return count; }
    int waitCount()  const { return static_cast<int>(wait_queue.size()); }

    void printLog() const
    {
        std::string border(OSConfig::TABLE_WIDTH, '-');
        std::cout << border << "\n";
        std::cout << "  Semaphore Log: " << resource_name << "\n";
        std::cout << border << "\n";
        for (const auto& entry : log)
            std::cout << "  " << entry << "\n";
        std::cout << border << "\n";
    }

    void reset()
    {
        count = max_count;
        while (!wait_queue.empty()) wait_queue.pop();
        log.clear();
    }
};


// ============================================================
// IORequest  —  Models a pending I/O operation for a process
// ============================================================
struct IORequest
{
    int         pid;
    int         io_finish_tick;  // clock tick when I/O completes
    std::string io_type;         // e.g. "DISK_READ", "NET_RECV"

    IORequest(int pid_, int finish_tick, const std::string& type)
        : pid(pid_), io_finish_tick(finish_tick), io_type(type) {}
};
