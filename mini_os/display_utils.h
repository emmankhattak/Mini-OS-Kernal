#pragma once
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <iomanip>
#include "config.h"
#include "pcb.h"

// ============================================================
// display_utils.h  —  Console Display Helpers
//
//  printBanner()       — decorative section header
//  printWorkloadInfo() — table of all processes in a workload
//  printOSConfig()     — dump the OSConfig constants
// ============================================================

class DisplayUtils
{
public:
    // ----------------------------------------------------------
    static void printBanner(const std::string& title, char symbol = '=')
    {
        int width      = OSConfig::TABLE_WIDTH + 4;
        std::string border(width, symbol);
        int title_len  = static_cast<int>(title.length());
        int padding    = (width - title_len - 2) / 2;

        std::cout << "\n" << border << "\n";
        std::cout << std::string(padding, symbol)
                  << " " << title << " "
                  << std::string(width - padding - title_len - 2, symbol)
                  << "\n";
        std::cout << border << "\n\n";
    }

    // ----------------------------------------------------------
    static void printWorkloadInfo(
        const std::vector<std::shared_ptr<PCB>>& processes,
        const std::string& title)
    {
        std::string border(OSConfig::TABLE_WIDTH, '-');
        std::cout << "\n" << border << "\n";
        std::cout << "  WORKLOAD: " << title << "\n";
        std::cout << "  Total Processes: " << processes.size() << "\n";
        std::cout << border << "\n";
        std::cout << std::left
                  << std::setw(6)  << "PID"
                  << std::setw(8)  << "Name"
                  << std::setw(10) << "Arrival"
                  << std::setw(10) << "Burst"
                  << std::setw(10) << "Priority"
                  << std::setw(8)  << "Pages"
                  << "\n";
        std::cout << border << "\n";

        for (const auto& pcb : processes)
            std::cout << std::left
                      << std::setw(6)  << pcb->pid
                      << std::setw(8)  << pcb->name
                      << std::setw(10) << pcb->arrival_time
                      << std::setw(10) << pcb->burst_time
                      << std::setw(10) << pcb->priority
                      << std::setw(8)  << pcb->num_pages
                      << "\n";
        std::cout << border << "\n";
    }

    // ----------------------------------------------------------
    static void printOSConfig()
    {
        std::string border(OSConfig::TABLE_WIDTH, '-');
        std::cout << "\n" << border << "\n";
        std::cout << "  SIMULATION CONFIGURATION\n";
        std::cout << border << "\n";
        std::cout << "  Physical Frames    : "
                  << OSConfig::TOTAL_PHYSICAL_FRAMES << "\n";
        std::cout << "  Page Size          : "
                  << OSConfig::PAGE_SIZE_BYTES << " bytes\n";
        std::cout << "  Physical Memory    : "
                  << OSConfig::TOTAL_PHYSICAL_FRAMES *
                     OSConfig::PAGE_SIZE_BYTES
                  << " bytes (" << OSConfig::TOTAL_PHYSICAL_FRAMES
                  << " frames)\n";
        std::cout << "  Max Pages/Process  : "
                  << OSConfig::PROCESS_ADDRESS_SPACE << "\n";
        std::cout << "  RR Time Quantum    : "
                  << OSConfig::RR_TIME_QUANTUM << " units\n";
        std::cout << "  Starvation Thresh  : "
                  << OSConfig::STARVATION_THRESHOLD << " units\n";
        std::cout << border << "\n";
    }
};
