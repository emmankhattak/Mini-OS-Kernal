#pragma once
#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include "config.h"

// ============================================================
// gantt_chart.h  —  ASCII Gantt Chart Renderer
//
//  addEntry(label, start, end) builds the chart incrementally.
//  Consecutive identical labels are merged automatically.
//  render() prints a boxed multi-row chart; rows wrap at 36 units.
// ============================================================

class GanttChart
{
public:
    struct GanttEntry
    {
        std::string label;
        int start_time;
        int end_time;
        int duration;

        GanttEntry(const std::string& lbl, int start, int end)
            : label(lbl), start_time(start),
              end_time(end), duration(end - start) {}
    };

private:
    std::vector<GanttEntry> entries;
    std::string chart_title;

    static constexpr int CHARS_PER_UNIT = 2;
    static constexpr int UNITS_PER_ROW  = 36;
    static constexpr int COLS_PER_ROW   = UNITS_PER_ROW * CHARS_PER_UNIT;

    // ----------------------------------------------------------
    void buildRowStrings(int row_start, int row_end,
                         std::string& bar_row,
                         std::string& label_row,
                         std::string& time_row) const
    {
        int cols = (row_end - row_start) * CHARS_PER_UNIT;
        bar_row.assign(cols, ' ');
        label_row.assign(cols, ' ');
        time_row.assign(cols, ' ');

        for (const auto& e : entries)
        {
            int es = std::max(e.start_time, row_start);
            int ee = std::min(e.end_time,   row_end);
            if (es >= ee) continue;

            int col_s = (es - row_start) * CHARS_PER_UNIT;
            int col_e = (ee - row_start) * CHARS_PER_UNIT;

            char bar_ch = (e.label == "IDLE") ? '-' : '=';
            for (int c = col_s; c < col_e - 1; ++c)
                bar_row[c] = bar_ch;
            bar_row[col_e - 1] = '|';

            if (e.start_time >= row_start && e.start_time < row_end)
            {
                int full_col_e  = std::min(e.end_time, row_end);
                int full_width  = (full_col_e - e.start_time) * CHARS_PER_UNIT;
                std::string lbl = e.label;
                if (static_cast<int>(lbl.size()) > full_width - 2)
                    lbl = lbl.substr(0, std::max(0, full_width - 2));
                int pad  = full_width - static_cast<int>(lbl.size());
                int left = pad / 2;
                int pos  = col_s + left;
                for (int k = 0; k < static_cast<int>(lbl.size()); ++k)
                    if (pos + k < cols)
                        label_row[pos + k] = lbl[k];
            }

            if (e.start_time >= row_start && e.start_time < row_end)
            {
                std::string ts = std::to_string(e.start_time);
                for (int k = 0; k < static_cast<int>(ts.size()); ++k)
                    if (col_s + k < cols)
                        time_row[col_s + k] = ts[k];
            }
        }

        // Print the final end-time tick of this row
        for (const auto& e : entries)
        {
            int ee = std::min(e.end_time, row_end);
            if (ee == row_end && ee > row_start)
            {
                int col       = (ee - row_start) * CHARS_PER_UNIT;
                std::string ts = std::to_string(ee);
                int pos       = col - static_cast<int>(ts.size());
                if (pos < 0) pos = 0;
                for (int k = 0;
                     k < static_cast<int>(ts.size()) && pos + k < cols; ++k)
                    time_row[pos + k] = ts[k];
                break;
            }
        }
    }

public:
    explicit GanttChart(const std::string& title) : chart_title(title) {}

    // ----------------------------------------------------------
    // Add a slice [start_time, end_time) for label.
    // Adjacent identical entries are merged automatically.
    void addEntry(const std::string& label, int start_time, int end_time)
    {
        if (start_time >= end_time) return;

        if (!entries.empty() &&
            entries.back().label    == label &&
            entries.back().end_time == start_time)
        {
            entries.back().end_time = end_time;
            entries.back().duration = end_time - entries.back().start_time;
        }
        else
        {
            entries.emplace_back(label, start_time, end_time);
        }
    }

    // ----------------------------------------------------------
    void render() const
    {
        if (entries.empty())
        {
            std::cout << "  [Gantt chart empty]\n";
            return;
        }

        int total_end   = entries.back().end_time;
        int total_start = entries.front().start_time;

        int box_inner = COLS_PER_ROW + 2;
        std::string h_top(box_inner, '=');
        std::string h_sep(box_inner, '-');

        std::cout << "\n  +" << h_top << "+\n";

        {
            std::string t = " GANTT CHART: " + chart_title;
            int pad = box_inner - static_cast<int>(t.size());
            if (pad < 0) { t = t.substr(0, box_inner); pad = 0; }
            std::cout << "  |" << t << std::string(pad, ' ') << "|\n";
        }
        std::cout << "  +" << h_sep << "+\n";

        int row_start = total_start;
        while (row_start < total_end)
        {
            int row_end = std::min(row_start + UNITS_PER_ROW, total_end);

            std::string bar_row, label_row, time_row;
            buildRowStrings(row_start, row_end, bar_row, label_row, time_row);

            auto pad_to = [&](std::string& s, int target)
            {
                if (static_cast<int>(s.size()) < target)
                    s.append(target - s.size(), ' ');
            };
            int row_cols = (row_end - row_start) * CHARS_PER_UNIT;
            pad_to(bar_row,   row_cols);
            pad_to(label_row, row_cols);
            pad_to(time_row,  row_cols);

            std::cout << "  | " << label_row << " |\n";
            std::cout << "  | " << bar_row   << " |\n";
            std::cout << "  | " << time_row  << " |\n";

            if (row_end < total_end)
                std::cout << "  |" << h_sep << "|\n";

            row_start = row_end;
        }
        std::cout << "  +" << h_top << "+\n\n";
    }

    // Accessors
    int getTotalTime() const
    {
        return entries.empty() ? 0 : entries.back().end_time;
    }

    int getIdleTime() const
    {
        int idle = 0;
        for (const auto& e : entries)
            if (e.label == "IDLE")
                idle += e.duration;
        return idle;
    }
};
