// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// What the axes of charts are made of: ranges of values, the ticks along them at round values (1, 2 or 5 times a power of
// ten; or, along time, at round times: every 5 seconds, every quarter of an hour, at midnight, on the first of a month),
// and their labels.
//
// Time is in seconds since 1970-01-01 UTC (chartTime makes it of a time point), as a double: a millisecond is told apart
// for 280 thousand years around it. Its labels are in local time.
//
// Labels are written in a locale (English's by default; a chart's, its window's language's: Chart::locale): its digits
// and marks, its shortened numbers (12.5k; ۱۲٫۵ هزار), its calendar's days, months and years (a Solar Hijri axis's
// months begin on the first of Farvardin, Ordibehesht...), its names of months.
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/Locale.hpp>

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace glw {

struct ChartRange {
    double low = 0;
    double high = 1;

    double span() const { return high - low; }
    bool contains(double v) const { return v >= low && v <= high; }
    friend bool operator==(const ChartRange&, const ChartRange&) = default;
};

// Ticks at round values in a range: as many as fit `count` (at least two apart), their step, and the decimals that tell
// them apart.
struct ChartTicks {
    std::vector<double> values;
    double step = 1;
    int decimals = 0;
};

GLW_EXPORT ChartTicks niceTicks(ChartRange range, int count);
// The range widened to the round values just around it, for `count` ticks: an axis fitted to data ends on its ticks.
GLW_EXPORT ChartRange niceRange(ChartRange range, int count);
// Ticks of a logarithmic axis (base 10) over a range above 0: at 1, 2 and 5 of each decade when that many fit `count`,
// otherwise at powers of ten (every decade, or every so many); within a decade, round values as niceTicks's. Its
// decimals: those of its smallest tick.
GLW_EXPORT ChartTicks logTicks(ChartRange range, int count);
// A range above 0 widened to 1, 2 or 5 times a power of ten at each end (a logarithmic axis fitted to data).
GLW_EXPORT ChartRange niceLogRange(ChartRange range);
// A value as a tick shows it: `decimals` after the point, digits grouped by thousands; past a million (and when the
// step is a thousand or more) shortened: 12.5k, 3M, 1.2G.
GLW_EXPORT std::string formatNumber(double value, int decimals, double step = 0, const Locale& locale = Locale::english());

// What the ticks of a time axis are a step of.
enum class TimeUnit : std::uint8_t { Millisecond, Second, Minute, Hour, Day, Month, Year };

struct TimeTicks {
    std::vector<double> values;     // seconds since 1970, at round local times
    TimeUnit unit = TimeUnit::Second;
    double step = 1;                // seconds (Month and Year: their count, months or years)
};

// Ticks at round local times in `seconds`, as many as fit `count` (at least two apart); months and years of the
// locale's calendar.
GLW_EXPORT TimeTicks timeTicks(ChartRange seconds, int count, const Locale& locale = Locale::english());
// A time as a tick of `ticks` shows it, in local time: 12:30:05.25, 12:30:05, 12:30, 6 Nov, Nov, 2026 (a month's tick
// in January its year); a tick at midnight shows its day.
GLW_EXPORT std::string formatTime(double seconds, const TimeTicks& ticks, const Locale& locale = Locale::english());
// ... in full, to the precision of `unit` (a tooltip's): 12:30:05.250, with its day before it when `date` (2026-11-06
// 12:30); Day and coarser, the day alone (2026-11-06, 2026-11, 2026).
GLW_EXPORT std::string formatTimeFully(double seconds, TimeUnit unit, bool date, const Locale& locale = Locale::english());

inline double chartTime(std::chrono::system_clock::time_point at) {
    return std::chrono::duration<double>(at.time_since_epoch()).count();
}
inline double chartNow() {
    return chartTime(std::chrono::system_clock::now());
}

} // namespace glw
