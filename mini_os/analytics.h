#pragma once
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include "enums.h"
#include "config.h"
#include "pcb.h"
#include "memory_manager.h"
#include "gantt_chart.h"

// ============================================================
// analytics.h  —  Metrics Computation + Comparative Table
//
//  computeAndPrint() calculates for a completed scheduling run:
//    - avg_waiting_time     = sum(WT) / n
//    - avg_turnaround_time  = sum(TAT) / n
//    - cpu_utilization      = (total - idle) / total * 100
//    - throughput           = n / total_time  (processes per unit)
//
//  printComparativeTable() lists all stored RunResults side-by-side
//  and highlights the best algorithm per metric.
// ============================================================

class AnalyticsEngine
{
public:
    struct RunResult
    {
        SchedulerType algorithm;
        std::string   algorithm_name;
        double avg_waiting_time;
        double avg_turnaround_time;
        double cpu_utilization;
        double throughput;
        int    total_page_faults;
        int    total_time;

        RunResult()
            : avg_waiting_time(0), avg_turnaround_time(0),
              cpu_utilization(0), throughput(0),
              total_page_faults(0), total_time(0) {}
    };

private:
    std::vector<RunResult> results;

public:
    // ----------------------------------------------------------
    RunResult computeAndPrint(
        const std::vector<std::shared_ptr<PCB>>& processes,
        const GanttChart&   gantt,
        SchedulerType       algo_type,
        const MemoryManager& mem_manager,
        const std::string&  workload_name)
    {
        RunResult result;
        result.algorithm      = algo_type;
        result.algorithm_name = schedulerTypeToString(algo_type);
        result.total_time     = gantt.getTotalTime();
        result.total_page_faults = mem_manager.getTotalPageFaults();

        double sum_wt = 0.0, sum_tat = 0.0;
        int n = static_cast<int>(processes.size());

        for (const auto& pcb : processes)
        {
            sum_wt  += pcb->waiting_time;
            sum_tat += pcb->turnaround_time;
        }

        result.avg_waiting_time    = (n > 0) ? sum_wt  / n : 0.0;
        result.avg_turnaround_time = (n > 0) ? sum_tat / n : 0.0;

        int idle_time = gantt.getIdleTime();
        result.cpu_utilization =
            (result.total_time > 0)
            ? (static_cast<double>(result.total_time - idle_time) /
               result.total_time * 100.0)
            : 0.0;
        result.throughput =
            (result.total_time > 0)
            ? static_cast<double>(n) / result.total_time
            : 0.0;

        // ---- Print ----
        std::string border(OSConfig::TABLE_WIDTH, '-');
        std::string bold(OSConfig::TABLE_WIDTH, '=');

        std::cout << "\n" << bold << "\n";
        std::cout << "  WORKLOAD: " << workload_name
                  << " | ALGORITHM: " << result.algorithm_name << "\n";
        std::cout << bold << "\n";

        std::cout << std::left
                  << std::setw(6)  << "PID"
                  << std::setw(8)  << "Name"
                  << std::setw(10) << "Arrival"
                  << std::setw(10) << "Burst"
                  << std::setw(10) << "Priority"
                  << std::setw(12) << "Completion"
                  << std::setw(12) << "Turnaround"
                  << std::setw(10) << "Waiting"
                  << std::setw(8)  << "PgFaults"
                  << "\n";
        std::cout << border << "\n";

        for (const auto& pcb : processes)
        {
            std::cout << std::left
                      << std::setw(6)  << pcb->pid
                      << std::setw(8)  << pcb->name
                      << std::setw(10) << pcb->arrival_time
                      << std::setw(10) << pcb->burst_time
                      << std::setw(10) << pcb->priority
                      << std::setw(12) << pcb->completion_time
                      << std::setw(12) << pcb->turnaround_time
                      << std::setw(10) << pcb->waiting_time
                      << std::setw(8)  << pcb->page_fault_count
                      << "\n";
        }
        std::cout << border << "\n";

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "  Avg Waiting Time       : "
                  << result.avg_waiting_time   << " time units\n";
        std::cout << "  Avg Turnaround Time    : "
                  << result.avg_turnaround_time << " time units\n";
        std::cout << "  CPU Utilization        : "
                  << result.cpu_utilization     << "%\n";
        std::cout << "  Throughput             : "
                  << result.throughput          << " proc/unit\n";
        std::cout << "  Total Page Faults      : "
                  << result.total_page_faults   << "\n";
        std::cout << "  Total FIFO Replacements: "
                  << mem_manager.getTotalPageReplacements() << "\n";
        std::cout << bold << "\n";

        results.push_back(result);
        return result;
    }

    // ----------------------------------------------------------
    void printComparativeTable(const std::string& workload_name) const
    {
        if (results.empty()) return;

        std::string border(OSConfig::TABLE_WIDTH, '=');
        std::string sub(OSConfig::TABLE_WIDTH, '-');

        std::cout << "\n" << border << "\n";
        std::cout << "  COMPARATIVE ANALYSIS: " << workload_name << "\n";
        std::cout << border << "\n";
        std::cout << std::left
                  << std::setw(35) << "Algorithm"
                  << std::setw(12) << "Avg WT"
                  << std::setw(12) << "Avg TAT"
                  << std::setw(12) << "CPU Util%"
                  << std::setw(12) << "Throughput"
                  << std::setw(10) << "PgFaults"
                  << "\n";
        std::cout << sub << "\n";

        for (const auto& r : results)
        {
            std::cout << std::fixed << std::setprecision(2) << std::left
                      << std::setw(35) << r.algorithm_name
                      << std::setw(12) << r.avg_waiting_time
                      << std::setw(12) << r.avg_turnaround_time
                      << std::setw(12) << r.cpu_utilization
                      << std::setw(12) << r.throughput
                      << std::setw(10) << r.total_page_faults
                      << "\n";
        }
        std::cout << border << "\n";

        auto best_wt   = std::min_element(results.begin(), results.end(),
            [](const RunResult& a, const RunResult& b)
            { return a.avg_waiting_time    < b.avg_waiting_time; });
        auto best_tat  = std::min_element(results.begin(), results.end(),
            [](const RunResult& a, const RunResult& b)
            { return a.avg_turnaround_time < b.avg_turnaround_time; });
        auto best_util = std::max_element(results.begin(), results.end(),
            [](const RunResult& a, const RunResult& b)
            { return a.cpu_utilization     < b.cpu_utilization; });

        std::cout << "\n  BEST ALGORITHM ANALYSIS:\n";
        std::cout << "  [*] Min Avg Waiting Time    : "
                  << best_wt->algorithm_name
                  << " (" << std::fixed << std::setprecision(2)
                  << best_wt->avg_waiting_time << " units)\n";
        std::cout << "  [*] Min Avg Turnaround Time : "
                  << best_tat->algorithm_name
                  << " (" << best_tat->avg_turnaround_time << " units)\n";
        std::cout << "  [*] Max CPU Utilization     : "
                  << best_util->algorithm_name
                  << " (" << best_util->cpu_utilization << "%)\n";
        std::cout << border << "\n\n";
    }

    void clearResults() { results.clear(); }
};
