#pragma once
#include <string>

// ============================================================
// enums.h  —  All Enumerations + String Converters
// ============================================================

enum class ProcessState  { NEW, READY, RUNNING, WAITING, TERMINATED };
enum class ThreadState   { NEW, READY, RUNNING, WAITING, TERMINATED };
enum class SchedulerType { FCFS, ROUND_ROBIN, SJF, PRIORITY };

// ----- String helpers ----------------------------------------

inline std::string stateToString(ProcessState s)
{
    switch (s)
    {
    case ProcessState::NEW:        return "NEW";
    case ProcessState::READY:      return "READY";
    case ProcessState::RUNNING:    return "RUNNING";
    case ProcessState::WAITING:    return "WAITING";
    case ProcessState::TERMINATED: return "TERMINATED";
    default:                       return "UNKNOWN";
    }
}

inline std::string threadStateToString(ThreadState s)
{
    switch (s)
    {
    case ThreadState::NEW:        return "NEW";
    case ThreadState::READY:      return "READY";
    case ThreadState::RUNNING:    return "RUNNING";
    case ThreadState::WAITING:    return "WAITING";
    case ThreadState::TERMINATED: return "TERMINATED";
    default:                      return "UNKNOWN";
    }
}

inline std::string schedulerTypeToString(SchedulerType t)
{
    switch (t)
    {
    case SchedulerType::FCFS:        return "First-Come-First-Serve (FCFS)";
    case SchedulerType::ROUND_ROBIN: return "Round Robin (RR, Q=3)";
    case SchedulerType::SJF:         return "Shortest Job First (SJF)";
    case SchedulerType::PRIORITY:    return "Priority Scheduling";
    default:                         return "UNKNOWN";
    }
}
