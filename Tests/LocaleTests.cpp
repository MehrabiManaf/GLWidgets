// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include "Check.hpp"

#include <GLWidgets/Charts/CandleChart.hpp>
#include <GLWidgets/Charts/ChartAxis.hpp>
#include <GLWidgets/Widgets/Locale.hpp>
#include <GLWidgets/Widgets/Localization.hpp>

#include <cmath>
#include <ctime>
#include <string>

using namespace glw;

namespace {

double midnight(int year, int month, int day) {
    std::tm t{};
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = day;
    t.tm_isdst = -1;
    return static_cast<double>(std::mktime(&t));
}

CivilDate localDay(double seconds) {
    const auto t = static_cast<std::time_t>(std::floor(seconds));
    std::tm out{};
#ifdef _WIN32
    localtime_s(&out, &t);
#else
    localtime_r(&t, &out);
#endif
    return {out.tm_year + 1900, out.tm_mon + 1, out.tm_mday};
}

} // namespace

TEST(theSolarHijriCalendarAgreesWithKnownDays) {
    // Nowruz, and a day of Aban
    CHECK((solarHijriFromGregorian({2025, 3, 21}) == CivilDate{1404, 1, 1}));
    CHECK((solarHijriFromGregorian({2024, 3, 20}) == CivilDate{1403, 1, 1}));
    CHECK((solarHijriFromGregorian({2026, 11, 6}) == CivilDate{1405, 8, 15}));
    CHECK((solarHijriFromGregorian({1979, 2, 11}) == CivilDate{1357, 11, 22}));
    CHECK((gregorianFromSolarHijri({1404, 1, 1}) == CivilDate{2025, 3, 21}));
    // 1403 is a leap year: its Esfand has a 30th, the day before Nowruz 1404
    CHECK(isSolarHijriLeapYear(1403) && !isSolarHijriLeapYear(1404));
    CHECK(solarHijriMonthDays(1403, 12) == 30 && solarHijriMonthDays(1404, 12) == 29 && solarHijriMonthDays(1404, 7) == 30);
    CHECK((gregorianFromSolarHijri({1403, 12, 30}) == CivilDate{2025, 3, 20}));
}

TEST(everyDayGoesToTheSolarHijriCalendarAndBack) {
    // from 1990 to 2040: there and back the same day, each the day after the one before
    CivilDate before = solarHijriFromGregorian({1989, 12, 31});
    bool ok = true;
    for (int year = 1990; year <= 2040 && ok; ++year)
        for (int month = 1; month <= 12 && ok; ++month) {
            const int days = month == 2 ? (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0) ? 29 : 28)
                             : month == 4 || month == 6 || month == 9 || month == 11 ? 30 : 31;
            for (int day = 1; day <= days && ok; ++day) {
                const CivilDate j = solarHijriFromGregorian({year, month, day});
                ok = gregorianFromSolarHijri(j) == CivilDate{year, month, day};
                const bool next = (j.year == before.year && j.month == before.month && j.day == before.day + 1) ||
                                  (j.year == before.year && j.month == before.month + 1 && j.day == 1 &&
                                   before.day == solarHijriMonthDays(before.year, before.month)) ||
                                  (j.year == before.year + 1 && j.month == 1 && j.day == 1 && before.month == 12);
                ok = ok && next;
                before = j;
            }
        }
    CHECK(ok);
}

TEST(aLocaleWritesNumbersInItsDigitsAndMarks) {
    const Locale fa = Locale::of("fa-IR");
    CHECK(fa.digits == Digits::Persian && fa.calendar == Calendar::SolarHijri && fa.firstDayOfWeek == 6);
    CHECK(fa.number(-1234.5, 1) == "\xE2\x80\x8E\xE2\x88\x92۱٬۲۳۴٫۵");
    CHECK(fa.number(1234567, 0, false) == "۱۲۳۴۵۶۷");
    CHECK(fa.digitsOf(8, 2) == "۰۸");
    CHECK(fa.formatDate(fa.date({2026, 11, 6})) == "۱۴۰۵/۰۸/۱۵");
    CHECK(Locale::of("ar-EG").number(12.5, 1) == "١٢٫٥" && Locale::of("ar-MA").number(12.5, 1) == "12.5");
    CHECK(Locale::english().number(-1234.5, 1) == "-1,234.5" && Locale::of("de").number(3) == "3");
    CHECK(Locale::of("en-US").firstDayOfWeek == 0 && Locale::of("en").firstDayOfWeek == 1);
}

TEST(countsTakeTheirLanguagesForms) {
    using P = PluralCategory;
    CHECK(pluralCategory("en", 1) == P::One && pluralCategory("en", 1, 1) == P::Other && pluralCategory("en", 0) == P::Other);
    CHECK(pluralCategory("fa", 0) == P::One && pluralCategory("fa", 1) == P::One && pluralCategory("fa", 2) == P::Other);
    CHECK(pluralCategory("fr", 1.5, 1) == P::One && pluralCategory("fr", 2) == P::Other);
    CHECK(pluralCategory("ar", 0) == P::Zero && pluralCategory("ar", 2) == P::Two && pluralCategory("ar", 3) == P::Few &&
          pluralCategory("ar", 11) == P::Many && pluralCategory("ar", 100) == P::Other);
    CHECK(pluralCategory("ru", 21) == P::One && pluralCategory("ru", 22) == P::Few && pluralCategory("ru", 25) == P::Many &&
          pluralCategory("ru", 12) == P::Many);
    CHECK(pluralCategory("pl", 1) == P::One && pluralCategory("pl", 22) == P::Few && pluralCategory("pl", 21) == P::Many);
    CHECK(pluralCategory("ja", 1) == P::Other);
}

TEST(aTranslatorChoosesACountsFormAndWritesItsNumbers) {
    Translator t;
    CHECK(t.loadCatalog(R"({"language": "ru", "texts": {"{n} file": {"one": "{n} файл", "few": "{n} файла", "many": "{n} файлов"}}})"));
    CHECK(t.loadCatalog(R"({"language": "fa", "texts": {"{n} file": {"other": "{n} فایل"}, "Variable {n}": "متغیر {n}"}})"));
    // English: its own two forms
    CHECK(t.resolve(trPlural("{n} file", "{n} files", 1)) == "1 file");
    CHECK(t.resolve(trPlural("{n} file", "{n} files", 1200)) == "1,200 files");
    t.setLanguage("ru");
    CHECK(t.resolve(trPlural("{n} file", "{n} files", 21)) == "21 файл" && t.resolve(trPlural("{n} file", "{n} files", 3)) == "3 файла" &&
          t.resolve(trPlural("{n} file", "{n} files", 5)) == "5 файлов");
    // a form it does not have: "other"'s, none: English's
    t.setLanguage("fa");
    CHECK(t.resolve(trPlural("{n} file", "{n} files", 1)) == "۱ فایل");
    CHECK(t.resolve(tr("Variable {n}").arg("n", 12)) == "متغیر ۱۲");
    // a locale of the application's, kept as the language changes
    Locale latin = Locale::of("fa");
    latin.digits = Digits::Latin;
    t.setLocale(latin);
    t.setLanguage("fa-IR");
    CHECK(t.resolve(tr("Variable {n}").arg("n", 12)) == "متغیر 12");
    t.setLocale(std::nullopt);
    CHECK(t.locale().digits == Digits::Persian);
    // forms that are not CLDR's: not a catalog
    std::string error;
    CHECK(!t.loadCatalog(R"({"language": "de", "texts": {"x": {"several": "y"}}})", &error) && !error.empty());
}

TEST(chartLabelsAreWrittenInTheirLocale) {
    const Locale fa = Locale::of("fa");
    CHECK(formatNumber(2500000, 0, 500000, fa) == "۲٫۵ میلیون" && formatNumber(13000, 0, 1000, fa) == "۱۳ هزار");
    CHECK(formatNumber(1500, 0, 0, fa) == "۱٬۵۰۰");
    CHECK(formatNumber(2500000, 0, 500000) == "2.5M" && formatNumber(13000, 0, 1000) == "13k");
    const double day = midnight(2026, 11, 6) + 12 * 3600 + 30 * 60;
    CHECK(formatTimeFully(day, TimeUnit::Minute, true, fa) == "۱۴۰۵/۰۸/۱۵ ۱۲:۳۰");
    CHECK(formatTimeFully(day, TimeUnit::Day, false) == "2026-11-06");
    const TimeTicks days = timeTicks({midnight(2026, 11, 6), midnight(2026, 11, 9)}, 4, fa);
    CHECK(days.unit == TimeUnit::Day && formatTime(days.values.front(), days, fa) == "۱۵ آبان");
}

TEST(aSolarHijriAxisHasItsTicksOnTheFirstsOfItsMonths) {
    const Locale fa = Locale::of("fa");
    const TimeTicks ticks = timeTicks({midnight(2025, 1, 1), midnight(2026, 1, 1)}, 13, fa);
    CHECK(ticks.unit == TimeUnit::Month && ticks.values.size() >= 11);
    bool firsts = true;
    for (const double v : ticks.values)
        firsts = firsts && fa.date(localDay(v)).day == 1;
    CHECK(firsts);
    // Farvardin's tick shows its year; another its month's name
    bool year = false, month = false;
    for (const double v : ticks.values) {
        const std::string label = formatTime(v, ticks, fa);
        year = year || label == "۱۴۰۴";
        month = month || label == "آبان";
    }
    CHECK(year && month);
}

TEST(aCandlesMonthIsOfItsCalendar) {
    // 2025-11-06 is 1404-08-15: its Solar Hijri month began on 1404-08-01, 2025-10-23; the next on 2025-11-22
    const double t = midnight(2025, 11, 6) + 3600;
    CHECK(CandlePeriod::months(1).start(t, 6, Calendar::SolarHijri) == midnight(2025, 10, 23));
    CHECK(CandlePeriod::months(1).next(midnight(2025, 10, 23), Calendar::SolarHijri) == midnight(2025, 11, 22));
    CHECK(CandlePeriod::years(1).start(t, 6, Calendar::SolarHijri) == midnight(2025, 3, 21));
    CHECK(CandlePeriod::months(1).start(t) == midnight(2025, 11, 1));
}
