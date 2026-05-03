#pragma once

// ============================================================
// config.h  —  Global Simulation Constants
// ============================================================

namespace OSConfig
{
    constexpr int TOTAL_PHYSICAL_FRAMES  = 16;
    constexpr int PAGE_SIZE_BYTES        = 256;
    constexpr int PROCESS_ADDRESS_SPACE  = 8;
    constexpr int RR_TIME_QUANTUM        = 3;
    constexpr int STARVATION_THRESHOLD   = 20;
    constexpr int GANTT_CELL_WIDTH       = 4;
    constexpr int TABLE_WIDTH            = 80;
    constexpr int PID_COLUMN_WIDTH       = 8;
    constexpr int METRIC_COLUMN_WIDTH    = 16;
}
