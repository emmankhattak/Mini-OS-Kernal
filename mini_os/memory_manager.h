#pragma once
#include <bitset>
#include <map>
#include <deque>
#include <vector>
#include <memory>
#include <cassert>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include "config.h"
#include "pcb.h"

// ============================================================
// memory_manager.h  —  Demand Paging + FIFO Page Replacement
//
//  How it works (viva summary):
//  1. Each process has a page_table (logical_page -> frame, -1 = unmapped).
//  2. On memory access, if the page is unmapped => PAGE FAULT.
//  3. Page fault handler:
//       a. Find a free physical frame (frame_bitmap).
//       b. If none free => FIFO eviction (oldest frame in fifo_queue).
//       c. Update frame_bitmap, frame_owner, fifo_queue, page_table.
//  4. releaseProcessMemory() called on process termination.
//
//  Data structures:
//    frame_bitmap  — bitset tracking which frames are occupied
//    frame_owner   — frame -> {pid, logical_page}
//    fifo_queue    — FIFO eviction order (front = oldest)
// ============================================================

class MemoryManager
{
private:
    std::bitset<OSConfig::TOTAL_PHYSICAL_FRAMES> frame_bitmap;
    std::map<int, std::pair<int,int>> frame_owner;  // frame -> {pid, page}
    std::deque<int> fifo_queue;
    int total_page_faults;
    int total_page_replacements;

    // ----------------------------------------------------------
    int findFreeFrame()
    {
        for (int i = 0; i < OSConfig::TOTAL_PHYSICAL_FRAMES; ++i)
            if (!frame_bitmap.test(i))
                return i;
        return -1;
    }

    // ----------------------------------------------------------
    int evictFIFO(std::vector<std::shared_ptr<PCB>>& process_pcbs,
                  int& evicted_pid, int& evicted_page)
    {
        int victim_frame = fifo_queue.front();
        fifo_queue.pop_front();

        auto owner_it = frame_owner.find(victim_frame);
        assert(owner_it != frame_owner.end() &&
               "FIFO eviction: frame has no owner record!");

        evicted_pid  = owner_it->second.first;
        evicted_page = owner_it->second.second;

        // Unmap the evicted page from its owner's page table
        for (auto& pcb : process_pcbs)
            if (pcb->pid == evicted_pid)
            {
                pcb->page_table[evicted_page] = -1;
                break;
            }

        frame_bitmap.reset(victim_frame);
        frame_owner.erase(victim_frame);
        ++total_page_replacements;
        return victim_frame;
    }

public:
    MemoryManager()
        : total_page_faults(0), total_page_replacements(0)
    {
        frame_bitmap.reset();
    }

    // ----------------------------------------------------------
    void handlePageFault(std::shared_ptr<PCB>& pcb,
                         int logical_page, int current_time,
                         std::vector<std::shared_ptr<PCB>>& all_pcbs)
    {
        ++total_page_faults;
        int free_frame   = findFreeFrame();
        int evicted_pid  = -1, evicted_page = -1;

        if (free_frame == -1)
            free_frame = evictFIFO(all_pcbs, evicted_pid, evicted_page);

        frame_bitmap.set(free_frame);
        frame_owner[free_frame] = {pcb->pid, logical_page};
        fifo_queue.push_back(free_frame);
        pcb->page_table[logical_page] = free_frame;
        pcb->logPageFault(current_time, logical_page,
                          free_frame, evicted_page);
    }

    // ----------------------------------------------------------
    // Returns the physical frame for logical_page; triggers fault if needed.
    int accessMemory(std::shared_ptr<PCB>& pcb, int logical_page,
                     int current_time,
                     std::vector<std::shared_ptr<PCB>>& all_pcbs)
    {
        if (logical_page < 0 || logical_page >= pcb->num_pages)
        {
            pcb->logEvent(current_time,
                "ERROR: Invalid logical page: " +
                std::to_string(logical_page));
            return -1;
        }
        int frame = pcb->page_table.at(logical_page);
        if (frame == -1)
        {
            handlePageFault(pcb, logical_page, current_time, all_pcbs);
            frame = pcb->page_table.at(logical_page);
        }
        return frame;
    }

    // ----------------------------------------------------------
    // Unmap all pages of a terminated process.
    void releaseProcessMemory(std::shared_ptr<PCB>& pcb)
    {
        for (auto& [page, frame] : pcb->page_table)
        {
            if (frame != -1)
            {
                fifo_queue.erase(
                    std::remove(fifo_queue.begin(), fifo_queue.end(), frame),
                    fifo_queue.end());
                frame_bitmap.reset(frame);
                frame_owner.erase(frame);
                frame = -1;
            }
        }
    }

    // ----------------------------------------------------------
    void reset()
    {
        frame_bitmap.reset();
        frame_owner.clear();
        fifo_queue.clear();
        total_page_faults       = 0;
        total_page_replacements = 0;
    }

    // ----------------------------------------------------------
    void printMemoryState() const
    {
        std::cout << "\n  Physical Memory State ("
                  << OSConfig::TOTAL_PHYSICAL_FRAMES << " Frames):\n  ";
        for (int i = 0; i < OSConfig::TOTAL_PHYSICAL_FRAMES; ++i)
        {
            if (frame_bitmap.test(i))
            {
                auto it = frame_owner.find(i);
                if (it != frame_owner.end())
                    std::cout << "[F" << std::setw(2) << i
                              << ":P" << it->second.first
                              << "p" << it->second.second << "]";
            }
            else
                std::cout << "[F" << std::setw(2) << i << ":FREE]";

            if ((i + 1) % 4 == 0)
                std::cout << "\n  ";
        }
        std::cout << "\n";
    }

    // Accessors
    int getTotalPageFaults()       const { return total_page_faults; }
    int getTotalPageReplacements() const { return total_page_replacements; }
    int getFreeFrameCount()        const
    {
        return OSConfig::TOTAL_PHYSICAL_FRAMES -
               static_cast<int>(frame_bitmap.count());
    }
    int getOccupiedFrameCount()    const
    {
        return static_cast<int>(frame_bitmap.count());
    }
};
