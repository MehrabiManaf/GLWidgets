// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/Locale.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <iterator>

namespace glw {

namespace {

// ---- the Solar Hijri calendar (jalaali-js's arithmetic: Borkowski's breaks of the 2820-year cycle) ----

constexpr int breaks[] = {-61, 9, 38, 199, 426, 686, 756, 818, 1111, 1181, 1210, 1635, 2060, 2097, 2192, 2262, 2324, 2394, 2456, 3178};

struct YearOf {
    int leap = 0;               // 0: a leap year (of the 33-year cycle's count)
    int gregorianYear = 0;      // the Gregorian year its Farvardin 1 is in
    int march = 0;              // the day of March its Farvardin 1 is
};

YearOf yearOf(int jy) {
    const int count = static_cast<int>(std::size(breaks));
    jy = std::clamp(jy, breaks[0], breaks[count - 1] - 1);         // (the years its breaks reach)
    const int gy = jy + 621;
    int leapJ = -14;
    int jp = breaks[0];
    int jump = 0;
    for (int i = 1; i < count; ++i) {
        const int jm = breaks[i];
        jump = jm - jp;
        if (jy < jm)
            break;
        leapJ += jump / 33 * 8 + jump % 33 / 4;
        jp = jm;
    }
    int n = jy - jp;
    leapJ += n / 33 * 8 + (n % 33 + 3) / 4;
    if (jump % 33 == 4 && jump - n == 4)
        ++leapJ;
    const int leapG = gy / 4 - (gy / 100 + 1) * 3 / 4 - 150;
    const int march = 20 + leapJ - leapG;
    if (jump - n < 6)
        n = n - jump + (jump + 4) / 33 * 33;
    int leap = ((n + 1) % 33 - 1) % 4;
    if (leap == -1)
        leap = 4;
    return {leap, gy, march};
}

// A Gregorian day's Julian day number, and back.
long long julianDay(long long gy, long long gm, long long gd) {
    long long d = (gy + (gm - 8) / 6 + 100100) * 1461 / 4 + (153 * ((gm + 9) % 12) + 2) / 5 + gd - 34840408;
    d = d - (gy + 100100 + (gm - 8) / 6) / 100 * 3 / 4 + 752;
    return d;
}

CivilDate gregorianOf(long long jdn) {
    long long j = 4 * jdn + 139361631;
    j = j + (4 * jdn + 183187720) / 146097 * 3 / 4 * 4 - 3908;
    const long long i = j % 1461 / 4 * 5 + 308;
    const int day = static_cast<int>(i % 153 / 5 + 1);
    const int month = static_cast<int>(i / 153 % 12 + 1);
    const int year = static_cast<int>(j / 1461 - 100100 + (8 - month) / 6);
    return {year, month, day};
}

// ---- digits ----

const char* digitOf(Digits digits, int d) {
    static const char* const persian[] = {"\xDB\xB0", "\xDB\xB1", "\xDB\xB2", "\xDB\xB3", "\xDB\xB4",
                                          "\xDB\xB5", "\xDB\xB6", "\xDB\xB7", "\xDB\xB8", "\xDB\xB9"};
    static const char* const arabic[] = {"\xD9\xA0", "\xD9\xA1", "\xD9\xA2", "\xD9\xA3", "\xD9\xA4",
                                         "\xD9\xA5", "\xD9\xA6", "\xD9\xA7", "\xD9\xA8", "\xD9\xA9"};
    static const char* const latin[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};
    return digits == Digits::Persian ? persian[d] : digits == Digits::Arabic ? arabic[d] : latin[d];
}

std::string primaryOf(std::string_view language) {
    std::string primary(language.substr(0, language.find_first_of("-_")));
    for (char& c : primary)
        c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
    return primary;
}

bool oneOf(std::string_view x, std::initializer_list<std::string_view> list) {
    return std::find(list.begin(), list.end(), x) != list.end();
}

} // namespace

CivilDate solarHijriFromGregorian(CivilDate g) {
    const long long jdn = julianDay(g.year, g.month, g.day);
    int jy = g.year - 621;
    const YearOf r = yearOf(jy);
    const long long first = julianDay(r.gregorianYear, 3, r.march);
    long long k = jdn - first;
    if (k >= 0) {
        if (k <= 185)
            return {jy, static_cast<int>(1 + k / 31), static_cast<int>(k % 31 + 1)};
        k -= 186;
    } else {
        --jy;
        k += 179;
        if (r.leap == 1)
            ++k;
    }
    return {jy, static_cast<int>(7 + k / 30), static_cast<int>(k % 30 + 1)};
}

CivilDate gregorianFromSolarHijri(CivilDate j) {
    const YearOf r = yearOf(j.year);
    const long long jdn = julianDay(r.gregorianYear, 3, r.march) + (j.month - 1) * 31LL - j.month / 7 * (j.month - 7) + j.day - 1;
    return gregorianOf(jdn);
}

bool isSolarHijriLeapYear(int year) {
    return yearOf(year).leap == 0;
}

int solarHijriMonthDays(int year, int month) {
    if (month <= 6)
        return 31;
    if (month <= 11)
        return 30;
    return isSolarHijriLeapYear(year) ? 30 : 29;
}

PluralCategory pluralCategory(std::string_view language, double count, int decimals) {
    using P = PluralCategory;
    const std::string lang = primaryOf(language);
    const double n = std::abs(count);
    const long long i = static_cast<long long>(std::floor(n));
    const int v = std::max(0, decimals);
    const bool whole = v == 0 && n == static_cast<double>(i);
    const long long i10 = i % 10, i100 = i % 100;
    // none: every count alike
    if (oneOf(lang, {"ja", "zh", "ko", "th", "vi", "id", "ms", "lo", "my", "km", "yue"}))
        return P::Other;
    // one: 0 and 1 (and what is between)
    if (oneOf(lang, {"fa", "hi", "bn", "gu", "kn", "zu", "am", "as"}))
        return i == 0 || n == 1 ? P::One : P::Other;
    if (oneOf(lang, {"fr", "pt", "hy", "ff", "kab"}))
        return i == 0 || i == 1 ? P::One : P::Other;
    // one: exactly 1, however written
    if (oneOf(lang, {"es", "tr", "el", "hu", "bg", "ka", "kk", "az", "uz", "ky", "mn", "ta", "te", "ml", "ne", "sq"}))
        return n == 1 ? P::One : P::Other;
    if (lang == "ar") {
        if (!whole)
            return P::Other;
        if (i == 0)
            return P::Zero;
        if (i == 1)
            return P::One;
        if (i == 2)
            return P::Two;
        if (i100 >= 3 && i100 <= 10)
            return P::Few;
        if (i100 >= 11 && i100 <= 99)
            return P::Many;
        return P::Other;
    }
    if (oneOf(lang, {"ru", "uk", "be"})) {
        if (v != 0)
            return P::Other;
        if (i10 == 1 && i100 != 11)
            return P::One;
        if (i10 >= 2 && i10 <= 4 && (i100 < 12 || i100 > 14))
            return P::Few;
        return P::Many;
    }
    if (lang == "pl") {
        if (v != 0)
            return P::Other;
        if (i == 1)
            return P::One;
        if (i10 >= 2 && i10 <= 4 && (i100 < 12 || i100 > 14))
            return P::Few;
        return P::Many;
    }
    if (oneOf(lang, {"cs", "sk"})) {
        if (v != 0)
            return P::Many;
        return i == 1 ? P::One : i >= 2 && i <= 4 ? P::Few : P::Other;
    }
    if (lang == "he" || lang == "iw") {
        if ((i == 1 && v == 0) || (i == 0 && v != 0))
            return P::One;
        return i == 2 && v == 0 ? P::Two : P::Other;
    }
    // English's: one is 1 written without decimals (en, de, nl, sv, it, ...)
    return i == 1 && v == 0 ? P::One : P::Other;
}

const Locale& Locale::english() {
    static const Locale locale;
    return locale;
}

Locale Locale::of(std::string_view tag) {
    Locale l;
    l.language = std::string(tag.empty() ? "en" : tag);
    const std::string primary = primaryOf(tag);
    if (primary == "fa") {
        l.digits = Digits::Persian;
        l.decimal = "\xD9\xAB";             // ٫
        l.group = "\xD9\xAC";               // ٬
        l.minus = "\xE2\x80\x8E\xE2\x88\x92";       // a left-to-right mark, −: before the number in right-to-left text
        l.percent = "\xD9\xAA";             // ٪
        l.compact = {" هزار", " میلیون", " میلیارد", " تریلیون"};
        l.calendar = Calendar::SolarHijri;
        l.firstDayOfWeek = 6;               // Saturday
        l.dateSeparator = "/";
        l.months = {"فروردین", "اردیبهشت", "خرداد", "تیر", "مرداد", "شهریور", "مهر", "آبان", "آذر", "دی", "بهمن", "اسفند"};
        l.shortMonths = l.months;
        l.weekdays = {"یکشنبه", "دوشنبه", "سه\xE2\x80\x8Cشنبه", "چهارشنبه", "پنجشنبه", "جمعه", "شنبه"};
        l.shortWeekdays = {"ی", "د", "س", "چ", "پ", "ج", "ش"};
    } else if (primary == "ar") {
        // the Maghreb writes Latin digits
        const std::string region = tag.size() > 3 ? std::string(tag.substr(3)) : std::string();
        const bool maghreb = region == "MA" || region == "DZ" || region == "TN" || region == "LY" || region == "EH";
        l.digits = maghreb ? Digits::Latin : Digits::Arabic;
        if (!maghreb) {
            l.decimal = "\xD9\xAB";
            l.group = "\xD9\xAC";
            l.percent = "\xD9\xAA\xD8\x9C";     // ٪ and an Arabic letter mark
        }
        l.minus = "\xD8\x9C-";              // an Arabic letter mark, -
        l.compact = {" ألف", " مليون", " مليار", " ترليون"};
        l.firstDayOfWeek = 6;
        l.dateSeparator = "/";
        l.months = {"يناير", "فبراير", "مارس", "أبريل", "مايو", "يونيو", "يوليو", "أغسطس", "سبتمبر", "أكتوبر", "نوفمبر", "ديسمبر"};
        l.shortMonths = l.months;
        l.weekdays = {"الأحد", "الاثنين", "الثلاثاء", "الأربعاء", "الخميس", "الجمعة", "السبت"};
        l.shortWeekdays = l.weekdays;
    } else if (primary == "en" && (tag == "en-US" || tag == "en_US")) {
        l.firstDayOfWeek = 0;
    }
    return l;
}

std::string Locale::localized(std::string_view ascii) const {
    if (digits == Digits::Latin && decimal == "." && group == "," && minus == "-" && percent == "%")
        return std::string(ascii);
    std::string out;
    out.reserve(ascii.size() * 2);
    for (const char c : ascii) {
        if (c >= '0' && c <= '9')
            out += digitOf(digits, c - '0');
        else if (c == '.')
            out += decimal;
        else if (c == ',')
            out += group;
        else if (c == '-')
            out += minus;
        else if (c == '%')
            out += percent;
        else
            out += c;
    }
    return out;
}

std::string Locale::number(double value, int decimals, bool grouped) const {
    if (!std::isfinite(value))
        return std::isnan(value) ? "\xE2\x80\x93" : value > 0 ? "\xE2\x88\x9E" : minus + "\xE2\x88\x9E";
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%.*f", std::clamp(decimals, 0, 12), value);
    std::string s = buffer;
    if (s[0] == '-' && std::strtod(s.c_str(), nullptr) == 0)        // -0.0
        s.erase(0, 1);
    if (grouped) {
        const std::size_t point = s.find('.');
        const std::size_t end = point == std::string::npos ? s.size() : point;
        const std::size_t begin = s[0] == '-' ? 1 : 0;
        for (std::size_t at = end; at > begin + 3; at -= 3)
            s.insert(at - 3, ",");
    }
    return localized(s);
}

std::string Locale::digitsOf(long long value, int width) const {
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%0*lld", std::clamp(width, 0, 20), value);
    std::string out;
    for (const char* c = buffer; *c; ++c) {
        if (*c >= '0' && *c <= '9')
            out += digitOf(digits, *c - '0');
        else if (*c == '-')
            out += minus;
        else
            out += *c;
    }
    return out;
}

CivilDate Locale::date(CivilDate g) const {
    return calendar == Calendar::SolarHijri ? solarHijriFromGregorian(g) : g;
}

CivilDate Locale::gregorian(CivilDate d) const {
    return calendar == Calendar::SolarHijri ? gregorianFromSolarHijri(d) : d;
}

std::string Locale::formatDate(CivilDate d) const {
    return digitsOf(d.year, 4) + dateSeparator + digitsOf(d.month, 2) + dateSeparator + digitsOf(d.day, 2);
}

} // namespace glw
