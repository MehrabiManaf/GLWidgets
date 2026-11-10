// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// How numbers, dates and counts are written in a language: its digits (Latin 0-9, Persian ۰-۹, Arabic ٠-٩), its decimal
// and grouping marks, its calendar (Gregorian, or the Solar Hijri calendar of Iran and Afghanistan), the names of its
// months and days, the day its weeks begin on, and which plural form a count takes (CLDR's rules: one, few, many...).
//
// A Translator has the locale of its language (Locale::of), unless one is set (Translator::setLocale): what widgets show
// numbers and dates with (UiContext::translator.locale()). A small set of languages is known in full (English, Persian,
// Arabic); the others have English's conventions and their own plural rules. ICU can come behind the same struct later.
//
//   const glw::Locale& fa = glw::Locale::of("fa");
//   fa.number(-1234.5, 1);                       // ‎−۱٬۲۳۴٫۵
//   fa.date({2026, 11, 6});                      // 1405-08-15 (Solar Hijri)
#include <GLWidgets/Export.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace glw {

enum class Digits : std::uint8_t {
    Latin,          // 0123456789
    Persian,        // ۰۱۲۳۴۵۶۷۸۹ (extended Arabic-Indic, U+06F0)
    Arabic,         // ٠١٢٣٤٥٦٧٨٩ (Arabic-Indic, U+0660)
};

enum class Calendar : std::uint8_t {
    Gregorian,
    SolarHijri,     // Iran's and Afghanistan's (Jalali): the year begins at the March equinox (Nowruz)
};

// Which form a count takes (CLDR's cardinal plural categories).
enum class PluralCategory : std::uint8_t { Zero, One, Two, Few, Many, Other };

// A day of a calendar: its year, its month (1 to 12), its day (1 to 31).
struct CivilDate {
    int year = 1970;
    int month = 1;
    int day = 1;
    friend bool operator==(const CivilDate&, const CivilDate&) = default;
};

// Between the Gregorian and the Solar Hijri calendars (the arithmetic of Borkowski's cycles of breaks, which agrees with
// the astronomical calendar from 1178 to 1633 AP: AD 1799 to 2254).
GLW_EXPORT CivilDate solarHijriFromGregorian(CivilDate gregorian);
GLW_EXPORT CivilDate gregorianFromSolarHijri(CivilDate solarHijri);
GLW_EXPORT bool isSolarHijriLeapYear(int year);
// Its days: 31 for the first six months, 30 for the next five, 29 for Esfand (30 in a leap year).
GLW_EXPORT int solarHijriMonthDays(int year, int month);

// The plural form of `count` in `language` (a BCP 47 tag), written with `decimals` digits after its point (1 and 1.0
// differ in some languages).
GLW_EXPORT PluralCategory pluralCategory(std::string_view language, double count, int decimals = 0);

struct GLW_EXPORT Locale {
    std::string language = "en";
    Digits digits = Digits::Latin;
    std::string decimal = ".";
    std::string group = ",";        // between thousands
    std::string minus = "-";
    std::string percent = "%";
    // After a number shortened to thousands, millions, billions, trillions: 12.5k, 3M (with a space when a word)
    std::array<std::string, 4> compact{"k", "M", "G", "T"};
    Calendar calendar = Calendar::Gregorian;
    int firstDayOfWeek = 1;         // 0 Sunday, 1 Monday ... 6 Saturday
    std::string dateSeparator = "-";        // between a date's year, month and day: 2026-11-06, ۱۴۰۵/۰۸/۱۵
    // its calendar's months, in full and short (an axis's); the days of the week from Sunday
    std::array<std::string, 12> months{"January", "February", "March", "April",   "May",      "June",
                                       "July",    "August",   "September", "October", "November", "December"};
    std::array<std::string, 12> shortMonths{"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    std::array<std::string, 7> weekdays{"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    std::array<std::string, 7> shortWeekdays{"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

    // English's (the default's).
    static const Locale& english();
    // A language's (a BCP 47 tag: "fa", "fa-IR", "ar-EG", "en-US"): Persian's and Arabic's in full, others English's
    // conventions with their own language (their plural rules); en-US's weeks from Sunday.
    static Locale of(std::string_view language);

    // What a number written with ASCII (its digits, '.', ',', '-', '%') is in it: its digits, its marks.
    std::string localized(std::string_view ascii) const;
    // A number with `decimals` digits after its point (rounded), its thousands grouped (when `grouped`).
    std::string number(double value, int decimals = 0, bool grouped = true) const;
    // A whole number's digits alone, `width` at least (zeros before it): a year, a day.
    std::string digitsOf(long long value, int width = 0) const;
    // The form a count takes in its language.
    PluralCategory plural(double count, int decimals = 0) const { return pluralCategory(language, count, decimals); }

    // A Gregorian day in its calendar, and back.
    CivilDate date(CivilDate gregorian) const;
    CivilDate gregorian(CivilDate date) const;
    // A day of its calendar written in full: its year, month and day by its separator (2026-11-06, ۱۴۰۵/۰۸/۱۵).
    std::string formatDate(CivilDate date) const;
};

} // namespace glw
