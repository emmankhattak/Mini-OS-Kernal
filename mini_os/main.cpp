// ============================================================
// main.cpp  —  Entry Point: Workload Setup + Simulation Driver
//
//  Workload 1: CPU-Bound  (6 processes, long bursts)
//  Workload 2: I/O-Bound  (8 processes, short bursts)
//
//  For each workload:
//    • runRoundRobinIO() — demonstrates I/O, mutex, TCB tracking
//    • runAllAlgorithms() — FCFS, RR, SJF, Priority + comparison
// ============================================================

#ifdef _WIN32
#include <windows.h>
#endif

#include <iostream>
#include <iomanip>
#include <memory>
#include <vector>

#include "config.h"
#include "pcb.h"
#include "memory_manager.h"
#include "scheduler.h"
#include "display_utils.h"

int main()
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    std::cout << std::fixed << std::setprecision(2);

    DisplayUtils::printBanner("MINI OS KERNEL SIMULATION", '*');
    std::cout
        << "  A Complete Process Scheduling & Memory Management Simulation\n"
        << "  Algorithms : FCFS | Round Robin | SJF | Priority\n"
        << "  Extensions : TCB | Mutex | Semaphore | I/O Waits\n"
        << "  Memory     : Paging with FIFO Page Replacement\n\n";
    DisplayUtils::printOSConfig();

    // ===========================================================
    // WORKLOAD 1: CPU-Bound Processes
    // ===========================================================
    DisplayUtils::printBanner("WORKLOAD 1: CPU-BOUND PROCESSES", '=');

    auto mem1 = std::make_shared<MemoryManager>();
    std::vector<std::shared_ptr<PCB>> cpu_procs;

    //                      PID  Name  Arrive Burst  Prio  Pages
    cpu_procs.push_back(std::make_shared<PCB>(1, "P1",  0, 20, 3, 6));
    cpu_procs.push_back(std::make_shared<PCB>(2, "P2",  2, 15, 1, 7));
    cpu_procs.push_back(std::make_shared<PCB>(3, "P3",  4, 25, 4, 8));
    cpu_procs.push_back(std::make_shared<PCB>(4, "P4",  6, 10, 2, 5));
    cpu_procs.push_back(std::make_shared<PCB>(5, "P5",  8, 18, 5, 6));
    cpu_procs.push_back(std::make_shared<PCB>(6, "P6", 10, 12, 2, 4));

    DisplayUtils::printWorkloadInfo(cpu_procs, "CPU-Bound");

    Scheduler sched1(cpu_procs, mem1, "CPU-Bound");
    sched1.runAllAlgorithms(5);   // dump PCB log for PID 5

    // ===========================================================
    // WORKLOAD 2: I/O-Bound / Mixed Processes
    // ===========================================================
    DisplayUtils::printBanner("WORKLOAD 2: I/O-BOUND / MIXED PROCESSES", '=');

    auto mem2 = std::make_shared<MemoryManager>();
    std::vector<std::shared_ptr<PCB>> io_procs;

    io_procs.push_back(std::make_shared<PCB>(101, "P1", 0, 4, 2, 2));
    io_procs.push_back(std::make_shared<PCB>(102, "P2", 0, 6, 3, 3));
    io_procs.push_back(std::make_shared<PCB>(103, "P3", 1, 3, 1, 2));
    io_procs.push_back(std::make_shared<PCB>(104, "P4", 2, 8, 4, 4));
    io_procs.push_back(std::make_shared<PCB>(105, "P5", 3, 2, 2, 2));
    io_procs.push_back(std::make_shared<PCB>(106, "P6", 4, 5, 3, 3));
    io_procs.push_back(std::make_shared<PCB>(107, "P7", 5, 7, 5, 3));
    io_procs.push_back(std::make_shared<PCB>(108, "P8", 6, 3, 1, 2));

    DisplayUtils::printWorkloadInfo(io_procs, "I/O-Bound / Mixed");

    Scheduler sched2(io_procs, mem2, "I/O-Bound/Mixed");

    // Extended RR with I/O + Mutex first
    DisplayUtils::printBanner(
        "ROUND ROBIN + I/O WAITS + MUTEX SYNCHRONIZATION", '-');
    sched2.runRoundRobinIO();

    // All four base algorithms + comparative table
    sched2.runAllAlgorithms(107);  // dump PCB log for PID 107

    // ===========================================================
    // Theoretical Summary
    // ===========================================================
    DisplayUtils::printBanner(
        "SIMULATION COMPLETE - THEORETICAL ANALYSIS", '*');

    std::cout
        << "  ALGORITHM CHARACTERISTICS SUMMARY:\n\n"

        << "  1. FCFS (First-Come-First-Serve)\n"
        << "     - Non-preemptive, simple FIFO queue\n"
        << "     - No starvation, but subject to Convoy Effect\n"
        << "     - Complexity: O(n log n) sort + O(n) execution\n\n"

        << "  2. Round Robin (Q=" << OSConfig::RR_TIME_QUANTUM << ")\n"
        << "     - Preemptive, circular FIFO queue\n"
        << "     - Extended: I/O waits, Mutex CS, TCB tick tracking\n"
        << "     - Fair CPU: max wait <= (n-1)*Q\n\n"

        << "  3. SJF (Shortest Job First) - Min-Heap\n"
        << "     - Provably optimal average waiting time\n"
        << "     - Heap: O(log n) insert/extract\n\n"

        << "  4. Priority Scheduling - Min-Heap\n"
        << "     - Starvation detection at threshold="
        << OSConfig::STARVATION_THRESHOLD << " units\n\n"

        << "  MEMORY MANAGEMENT:\n"
        << "     - Demand Paging: pages loaded only on access\n"
        << "     - FIFO Replacement: evict oldest frame on fault\n\n"

        << "  SYNCHRONIZATION:\n"
        << "     - SimMutex:     binary lock, FIFO waiter queue\n"
        << "     - SimSemaphore: counting lock, N concurrent holders\n"
        << "     - Deadlock: avoided by single-resource acquisition order\n\n"

        << "  THREAD MODEL:\n"
        << "     - Each PCB owns >= 1 TCB (main thread auto-created)\n"
        << "     - TCBs share parent page table (user-level thread model)\n"
        << "     - TCB state mirrors ProcessState for main thread\n\n";

    DisplayUtils::printBanner("END OF SIMULATION", '*');
    return 0;
}
