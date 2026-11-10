// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Charts/ChartAxis.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <utility>

namespace glw {

namespace {

constexpr int mostTicks = 1000;

// The number of decimals that tell values `step` apart.
int decimalsFor(double step) {
    if (!(step > 0) || !std::isfinite(step))
        return 0;
    return std::clamp(static_cast<int>(-std::floor(std::log10(step) + 1e-9)), 0, 12);
}

// A round step near `raw`: 1, 2 or 5 times a power of ten, not less than it.
double niceStep(double raw) {
    const double magnitude = std::pow(10.0, std::floor(std::log10(raw)));
    const double f = raw / magnitude;
    const double nice = f <= 1 + 1e-9 ? 1 : f <= 2 + 1e-9 ? 2 : f <= 5 + 1e-9 ? 5 : 10;
    return nice * magnitude;
}

std::tm localTime(std::time_t t) {
    std::tm out{};
#ifdef _WIN32
    localtime_s(&out, &t);
#else
    localtime_r(&t, &out);
#endif
    return out;
}

// Days from 1970-01-01 to a date of the proleptic Gregorian calendar (Howard Hinnant's days_from_civil).
long long daysFromCivil(long long y, unsigned m, unsigned d) {
    y -= m <= 2;
    const long long era = (y >= 0 ? y : y - 399) / 400;
    const auto yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<long long>(doe) - 719468;
}

// How far local time is ahead of UTC at `t`, in seconds.
double utcOffset(double t) {
    const auto whole = static_cast<std::time_t>(std::floor(t));
    const std::tm l = localTime(whole);
    const long long asUtc = daysFromCivil(l.tm_year + 1900LL, static_cast<unsigned>(l.tm_mon + 1), static_cast<unsigned>(l.tm_mday)) * 86400 +
                            l.tm_hour * 3600LL + l.tm_min * 60LL + l.tm_sec;
    return static_cast<double>(asUtc - static_cast<long long>(whole));
}

struct TimeStep {
    double seconds;     // Month and Year: months, years
    TimeUnit unit;
};

constexpr std::array<TimeStep, 38> timeSteps{{
    {0.001, TimeUnit::Millisecond}, {0.002, TimeUnit::Millisecond}, {0.005, TimeUnit::Millisecond},
    {0.01, TimeUnit::Millisecond},  {0.02, TimeUnit::Millisecond},  {0.05, TimeUnit::Millisecond},
    {0.1, TimeUnit::Millisecond},   {0.2, TimeUnit::Millisecond},   {0.5, TimeUnit::Millisecond},
    {1, TimeUnit::Second},          {2, TimeUnit::Second},          {5, TimeUnit::Second},
    {10, TimeUnit::Second},         {15, TimeUnit::Second},         {30, TimeUnit::Second},
    {60, TimeUnit::Minute},         {120, TimeUnit::Minute},        {300, TimeUnit::Minute},
    {600, TimeUnit::Minute},        {900, TimeUnit::Minute},        {1800, TimeUnit::Minute},
    {3600, TimeUnit::Hour},         {7200, TimeUnit::Hour},         {10800, TimeUnit::Hour},
    {21600, TimeUnit::Hour},        {43200, TimeUnit::Hour},        {86400, TimeUnit::Day},
    {172800, TimeUnit::Day},        {604800, TimeUnit::Day},        {1, TimeUnit::Month},
    {2, TimeUnit::Month},           {3, TimeUnit::Month},           {6, TimeUnit::Month},
    {1, TimeUnit::Year},            {2, TimeUnit::Year},            {5, TimeUnit::Year},
    {10, TimeUnit::Year},           {100, TimeUnit::Year},
}};

double approximateSeconds(const TimeStep& s) {
    return s.unit == TimeUnit::Month ? s.seconds * 30.44 * 86400 : s.unit == TimeUnit::Year ? s.seconds * 365.25 * 86400 : s.seconds;
}

// A local time's day in the locale's calendar.
CivilDate dayOf(const std::tm& t, const Locale& locale) {
    return locale.date({t.tm_year + 1900, t.tm_mon + 1, t.tm_mday});
}

std::string dayLabel(const std::tm& t, const Locale& locale) {
    const CivilDate d = dayOf(t, locale);
    return locale.digitsOf(d.day) + " " + locale.shortMonths[static_cast<std::size_t>(std::clamp(d.month, 1, 12) - 1)];
}

// Digits grouped by thousands: 1234567 is 1,234,567.
std::string grouped(std::string digits) {
    const std::size_t point = digits.find('.');
    const std::size_t end = point == std::string::npos ? digits.size() : point;
    const std::size_t begin = !digits.empty() && digits[0] == '-' ? 1 : 0;
    for (std::size_t at = end; at > begin + 3; at -= 3)
        digits.insert(at - 3, ",");
    return digits;
}

} // namespace

ChartTicks niceTicks(ChartRange range, int count) {
    ChartTicks ticks;
    count = std::max(count, 2);
    const double span = range.span();
    if (!(span > 0) || !std::isfinite(span)) {
        if (std::isfinite(range.low))
            ticks.values.push_back(range.low);
        return ticks;
    }
    ticks.step = niceStep(span / (count - 1));
    // at least two in it: a smaller round step when the first is about as long as all of it (10, 5, 2, 1...)
    for (int smaller = 0; smaller < 6 && std::floor(range.high / ticks.step + 1e-9) - std::ceil(range.low / ticks.step - 1e-9) < 1; ++smaller) {
        const double magnitude = std::pow(10.0, std::floor(std::log10(ticks.step) + 1e-9));
        const double f = ticks.step / magnitude;
        ticks.step = (f > 5.5 ? 5 : f > 2.5 ? 2 : f > 1.5 ? 1 : 0.5) * magnitude;
    }
    ticks.decimals = decimalsFor(ticks.step);
    const double first = std::ceil(range.low / ticks.step - 1e-9);
    for (int i = 0; i < mostTicks; ++i) {
        double v = (first + i) * ticks.step;
        if (v > range.high + ticks.step * 1e-9)
            break;
        if (std::abs(v) < ticks.step * 1e-9)
            v = 0;
        ticks.values.push_back(v);
    }
    return ticks;
}

ChartRange niceRange(ChartRange range, int count) {
    if (!(range.span() > 0) || !std::isfinite(range.span()))
        return range;
    const double step = niceStep(range.span() / (std::max(count, 2) - 1));
    ChartRange out{std::floor(range.low / step + 1e-9) * step, std::ceil(range.high / step - 1e-9) * step};
    if (out.high <= out.low)
        out.high = out.low + step;
    return out;
}

std::string formatNumber(double value, int decimals, double step, const Locale& locale) {
    if (!std::isfinite(value))
        return std::isnan(value) ? "\xE2\x80\x93" : value > 0 ? "\xE2\x88\x9E" : locale.minus + "\xE2\x88\x9E";
    const double size = std::max(std::abs(value), std::abs(step));
    char out[64];
    if (size >= 1e6 || (std::abs(step) >= 1000 && std::abs(value) >= 1000)) {
        static constexpr std::pair<double, std::size_t> scales[] = {{1e12, 3}, {1e9, 2}, {1e6, 1}, {1e3, 0}};
        for (const auto& [scale, suffix] : scales) {
            if (size >= scale) {
                const int d = step > 0 ? decimalsFor(step / scale) : std::min(decimals, 1);
                std::snprintf(out, sizeof out, "%.*f", d, value / scale);
                std::string s = out;
                if (s.rfind("-0", 0) == 0 && std::strtod(s.c_str(), nullptr) == 0)
                    s.erase(0, 1);
                return locale.localized(s) + locale.compact[suffix];
            }
        }
    }
    std::snprintf(out, sizeof out, "%.*f", std::clamp(decimals, 0, 12), value);
    std::string s = out;
    if (s[0] == '-' && std::strtod(s.c_str(), nullptr) == 0)        // -0.0
        s.erase(0, 1);
    return locale.localized(grouped(std::move(s)));
}

TimeTicks timeTicks(ChartRange range, int count, const Locale& locale) {
    TimeTicks ticks;
    count = std::max(count, 2);
    const double span = range.span();
    if (!(span > 0) || !std::isfinite(span)) {
        if (std::isfinite(range.low))
            ticks.values.push_back(range.low);
        return ticks;
    }
    // the smallest step that keeps them `count` at most
    const TimeStep* chosen = &timeSteps.back();
    for (const TimeStep& s : timeSteps) {
        if (span / approximateSeconds(s) <= count - 1) {
            chosen = &s;
            break;
        }
    }
    ticks.unit = chosen->unit;
    ticks.step = chosen->seconds;
    if ((chosen->unit == TimeUnit::Month || chosen->unit == TimeUnit::Year) && locale.calendar != Calendar::Gregorian) {
        // on the first of a month of the locale's calendar (of a year), a whole number of steps into its year (its
        // century): months counted from its year 0, each made a Gregorian day again
        const int n = static_cast<int>(chosen->seconds);
        const CivilDate at = dayOf(localTime(static_cast<std::time_t>(std::floor(range.low))), locale);
        long long month = chosen->unit == TimeUnit::Month ? at.year * 12LL + (at.month - 1) - (at.month - 1) % n
                                                          : (at.year - ((at.year % n) + n) % n) * 12LL;
        for (int i = 0; i < mostTicks + 2; ++i) {
            const CivilDate g = locale.gregorian({static_cast<int>(month / 12), static_cast<int>(month % 12) + 1, 1});
            std::tm t{};
            t.tm_year = g.year - 1900;
            t.tm_mon = g.month - 1;
            t.tm_mday = g.day;
            t.tm_isdst = -1;
            const double v = static_cast<double>(std::mktime(&t));
            if (v > range.high)
                break;
            if (v >= range.low)
                ticks.values.push_back(v);
            month += chosen->unit == TimeUnit::Month ? n : n * 12LL;
        }
        return ticks;
    }
    if (chosen->unit == TimeUnit::Month || chosen->unit == TimeUnit::Year) {
        // on the first of a month (of a year), a whole number of steps into its year (its century)
        std::tm t = localTime(static_cast<std::time_t>(std::floor(range.low)));
        t.tm_mday = 1;
        t.tm_hour = t.tm_min = t.tm_sec = 0;
        t.tm_isdst = -1;
        const int n = static_cast<int>(chosen->seconds);
        if (chosen->unit == TimeUnit::Month) {
            t.tm_mon -= t.tm_mon % n;
        } else {
            t.tm_mon = 0;
            t.tm_year -= (t.tm_year + 1900) % n;
        }
        for (int i = 0; i < mostTicks + 2; ++i) {
            std::tm at = t;
            at.tm_isdst = -1;
            const double v = static_cast<double>(std::mktime(&at));
            if (v > range.high)
                break;
            if (v >= range.low)
                ticks.values.push_back(v);
            if (chosen->unit == TimeUnit::Month)
                t.tm_mon += n;
            else
                t.tm_year += n;
        }
        return ticks;
    }
    // round in local time: a whole number of steps from local midnight
    const double offset = utcOffset(range.low);
    const double step = chosen->seconds;
    const double first = std::ceil((range.low + offset) / step - 1e-9);
    for (int i = 0; i < mostTicks; ++i) {
        const double v = (first + i) * step - offset;
        if (v > range.high + step * 1e-9)
            break;
        ticks.values.push_back(v);
    }
    return ticks;
}

std::string formatTime(double seconds, const TimeTicks& ticks, const Locale& locale) {
    if (!std::isfinite(seconds))
        return "\xE2\x80\x93";
    const double whole = std::floor(seconds);
    const double fraction = seconds - whole;
    const std::tm t = localTime(static_cast<std::time_t>(whole));
    const bool midnight = t.tm_hour == 0 && t.tm_min == 0 && t.tm_sec == 0 && fraction < 1e-6;
    char out[64];
    switch (ticks.unit) {
    case TimeUnit::Millisecond: {
        const int decimals = ticks.step >= 0.1 - 1e-9 ? 1 : ticks.step >= 0.01 - 1e-9 ? 2 : 3;
        const int scale = decimals == 1 ? 10 : decimals == 2 ? 100 : 1000;
        const int part = std::min(scale - 1, static_cast<int>(std::lround(fraction * scale)));
        std::snprintf(out, sizeof out, "%02d:%02d:%02d.%0*d", t.tm_hour, t.tm_min, t.tm_sec, decimals, part);
        return locale.localized(out);
    }
    case TimeUnit::Second:
        if (midnight)
            return dayLabel(t, locale);
        std::snprintf(out, sizeof out, "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
        return locale.localized(out);
    case TimeUnit::Minute:
    case TimeUnit::Hour:
        if (midnight)
            return dayLabel(t, locale);
        std::snprintf(out, sizeof out, "%02d:%02d", t.tm_hour, t.tm_min);
        return locale.localized(out);
    case TimeUnit::Day:
        return dayLabel(t, locale);
    case TimeUnit::Month: {
        // the first month of its year: the year
        const CivilDate d = dayOf(t, locale);
        if (d.month == 1)
            return locale.digitsOf(d.year);
        return locale.shortMonths[static_cast<std::size_t>(std::clamp(d.month, 1, 12) - 1)];
    }
    case TimeUnit::Year:
        return locale.digitsOf(dayOf(t, locale).year);
    }
    return {};
}

std::string formatTimeFully(double seconds, TimeUnit unit, bool date, const Locale& locale) {
    if (!std::isfinite(seconds))
        return "\xE2\x80\x93";
    const double whole = std::floor(seconds);
    const std::tm t = localTime(static_cast<std::time_t>(whole));
    const CivilDate d = dayOf(t, locale);
    const std::string& to = locale.dateSeparator;
    char time[48];          // (room for any int: a compiler counts them so)
    switch (unit) {
    case TimeUnit::Year:
        return locale.digitsOf(d.year, 4);
    case TimeUnit::Month:
        return locale.digitsOf(d.year, 4) + to + locale.digitsOf(d.month, 2);
    case TimeUnit::Day:
        return locale.formatDate(d);
    case TimeUnit::Millisecond:
        std::snprintf(time, sizeof time, "%02d:%02d:%02d.%03d", t.tm_hour, t.tm_min, t.tm_sec,
                      std::min(999, static_cast<int>(std::lround((seconds - whole) * 1000))));
        break;
    case TimeUnit::Second:
        std::snprintf(time, sizeof time, "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
        break;
    case TimeUnit::Minute:
    case TimeUnit::Hour:
        std::snprintf(time, sizeof time, "%02d:%02d", t.tm_hour, t.tm_min);
        break;
    }
    return date ? locale.formatDate(d) + " " + locale.localized(time) : locale.localized(time);
}

} // namespace glw
