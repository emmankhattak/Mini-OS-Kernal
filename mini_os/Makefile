# ============================================================
# Makefile  —  Mini OS Kernel Simulator
# ============================================================

CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -Iinclude
TARGET   := mini_os

SRCS := src/main.cpp \
        src/scheduler_io.cpp

# All headers (for dependency tracking)
HDRS := include/config.h         \
        include/enums.h          \
        include/tcb.h            \
        include/pcb.h            \
        include/memory_manager.h \
        include/sync.h           \
        include/gantt_chart.h    \
        include/analytics.h      \
        include/scheduler.h      \
        include/display_utils.h

all: $(TARGET)

$(TARGET): $(SRCS) $(HDRS)
	$(CXX) $(CXXFLAGS) -o $@ $(SRCS)

run: $(TARGET)
	./$(TARGET)

# Pipe through less for comfortable reading
view: $(TARGET)
	./$(TARGET) 2>&1 | less

clean:
	rm -f $(TARGET)

help:
	@echo ""
	@echo "  Mini OS Kernel Simulator — Build Targets"
	@echo "  -----------------------------------------"
	@echo "  make          Build the simulator"
	@echo "  make run      Build and run"
	@echo "  make view     Build, run, and page output with less"
	@echo "  make clean    Remove binary"
	@echo ""
	@echo "  Source files:"
	@echo "    src/main.cpp          — workload setup + driver"
	@echo "    src/scheduler_io.cpp  — RR + I/O + Mutex (extended)"
	@echo ""
	@echo "  Key headers:"
	@echo "    include/config.h         — global constants"
	@echo "    include/enums.h          — ProcessState / ThreadState / SchedulerType"
	@echo "    include/tcb.h            — Thread Control Block"
	@echo "    include/pcb.h            — Process Control Block"
	@echo "    include/memory_manager.h — demand paging + FIFO replacement"
	@echo "    include/sync.h           — SimMutex / SimSemaphore / IORequest"
	@echo "    include/gantt_chart.h    — ASCII Gantt renderer"
	@echo "    include/analytics.h      — metrics + comparative table"
	@echo "    include/scheduler.h      — FCFS / RR / SJF / Priority"
	@echo "    include/display_utils.h  — console display helpers"
	@echo ""

.PHONY: all run view clean help
