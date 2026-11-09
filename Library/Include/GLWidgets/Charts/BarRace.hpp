// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// A bar chart race: items ranked by their values as bars lying from the start side, the highest at the top. All of them
// are in it, a few rows of it shown at once and the rest scrolled to. As the values change (a period after another: a
// year, a month, a second), the bars slide to their new places and grow or shrink to their new lengths; the value axis
// is fitted to the highest bar that shows, its ticks gliding with it, and the period shows large in a corner. A bar may be
// stacked of parts (what each item's value is made of: the legend names them, and a click on one takes it out of every
// bar, eased, and the ranks again).
//
// Heavy data: an item's values change in constant time and only mark it to be painted; once a frame all of them are
// ranked again from the ranking they had (most move a little if at all: an insertion sort, nearly linear; a sort when they
// are all over the place; ties kept in the order they were), their values and places kept side by side in arrays, and
// only the rows that show are painted, so a race of thousands of items, all changing many times a second, costs about what one of a
// dozen does. A change shows over as long as the time since the one before (a race flows on at the pace its data comes,
// linearly), or over `Props::pace`.
//
// Only the top ones (heavy data, at a glance): `Props::top` of them ranked and shown, the rest together as one bar of
// others under them (a neutral gray, its parts' sums, its count; the scale fitted to the top ones, so that it is cut at
// the end of the plot when it is longer). They are found by a partial sort (n log k); one that comes into the top rises out
// of the others' bar, one that leaves sinks into it.
//
// The wheel scrolls through the ranks (eased), with Ctrl it shows more rows or fewer; a drag scrolls too, and a double
// click goes back to the top, as many rows as at first. Its scroll bar is the style's, as a scroll area's: its thumb
// dragged, its track pressed for a page. The bar under the
// mouse is lit, its parts, total and rank by the mouse.
#include <GLWidgets/Charts/Chart.hpp>
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/ScrollBar.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace glw {

class GLW_EXPORT BarRace : public Chart {
public:
    struct Props {
        LocalizedText title;
        std::size_t shown = 10;                     // the rows shown at once (the others scrolled to)
        std::vector<LocalizedText> parts;           // what each bar is stacked of, in order (the legend); none: one part
        std::chrono::milliseconds pace{0};          // how long a change takes to show; 0: as long as since the one before
        bool totals = true;                         // each bar's total at its end
        std::size_t top = 0;                        // only the top this many ranked, the rest one bar of others; 0: all
        LocalizedText others = "Others";
    };

    // What itemAt gives for the others' bar.
    static constexpr std::size_t othersItem = static_cast<std::size_t>(-1);

    BarRace();
    explicit BarRace(Props props);
    ~BarRace() override;

    // An item (its values 0): its index. Its color: its own, or the style's in turn (a bar of parts: the parts' colors).
    std::size_t addItem(LocalizedText name, std::optional<Color> color = std::nullopt);
    std::size_t itemCount() const { return names_.size(); }
    const LocalizedText& itemName(std::size_t item) const { return names_[item]; }
    // Its value (a bar of one part) / its parts' values, in the order of Props::parts.
    void setValue(std::size_t item, double value);
    void setValues(std::size_t item, std::span<const double> parts);
    // Its value: its parts' that show, together.
    double total(std::size_t item) const;
    // What its values are of now: shown large in a corner (a year, a date).
    void setPeriod(LocalizedText period);
    const LocalizedText& period() const { return period_; }
    void setShown(std::size_t rows);
    void setTop(std::size_t top);
    // How many items the others' bar is of now (0: none, all ranked).
    std::size_t othersCount() const { return othersCount_; }
    std::size_t shown() const { return props_.shown; }
    void setPartVisible(std::size_t part, bool visible);
    bool partVisible(std::size_t part) const { return part >= hidden_.size() || !hidden_[part]; }
    // How a value shows (a tick, a step apart from the next; a bar's total and a tooltip's: step 0); none: formatNumber.
    void setFormatter(std::function<std::string(double value, double step)> format);

    // ---- the view ----

    // The rank shown at the top (fractions: a part of a row), eased there / at once.
    void scrollTo(double rank, bool animated = true);
    double scrollPosition() const { return scroll_; }

    // All the items as their values ranked them when it was last painted (the highest first; equals in the order they
    // were): what it goes to.
    std::vector<std::size_t> ranking() const;
    // As it shows now (eased): an item's total, and its place (0: the first rank).
    double shownTotal(std::size_t item) const;
    float shownPlace(std::size_t item) const;
    // The item whose bar is at a point (its own coordinates), as it was last painted.
    std::optional<std::size_t> itemAt(Point at) const;

protected:
    void paint(PaintContext& context) override;
    void onMouse(MouseEvent& event) override;
    void onHoverChanged(bool hovered) override;
    std::optional<platform::Cursor> cursorAt(Point point) const override;
    std::vector<LegendItem> legendItems() const override;
    void legendItemClicked(std::size_t index) override;

private:
    std::size_t partCount() const { return std::max<std::size_t>(1, props_.parts.size()); }
    double target(std::size_t item, std::size_t part) const;
    double shownPart(std::size_t item, std::size_t part) const;
    // What changed eased to, from what shows: all ranked again (once a frame).
    void settle();
    std::string format(double value, double step) const;
    Color colorOf(std::size_t item, std::size_t part) const;
    double lastTop() const;                         // the highest rank the view scrolls to
    std::size_t rowCount() const;                   // the bars it has: the ranked ones, and the others'
    double shownOthers(std::size_t part) const;

    Props props_;
    std::size_t firstShown_ = 10;
    // the items, side by side: a race of many is read through in a frame
    std::vector<LocalizedText> names_;
    std::vector<std::optional<Color>> colors_;
    std::vector<double> values_;                    // each item's parts', item by item
    std::vector<double> from_, to_;                 // ... as they show, eased between
    std::vector<float> fromPlace_, toPlace_;
    std::vector<std::uint32_t> rank_;               // its place in the ranking it goes to
    std::vector<char> ranked_;                      // in a ranking yet (a new one shows where it goes, not from elsewhere)
    std::vector<std::uint32_t> order_;              // the ranking: the items by rank
    struct Key {
        double total;
        std::uint32_t rank, item;
    };
    std::vector<Key> keys_;                         // scratch: the ranking made again
    bool shuffled_ = false;                         // the last ranking was all over the place: sorted at once
    std::size_t limit_ = 0;                         // ranked: all, or the top ones
    std::size_t othersCount_ = 0;
    std::vector<double> othersFrom_, othersTo_;     // the others' parts, eased between
    std::vector<bool> hidden_;                      // parts
    LocalizedText period_;
    std::function<std::string(double, double)> format_;
    bool dirty_ = false;
    Animation progress_;
    std::optional<TimePoint> lastChange_;           // when the last change showed: the pace of the next
    double scroll_ = 0;                             // the rank at the top
    SmoothScroll scrolling_;
    float slot_ = 1;                                // a row's height, as last painted
    Rect plot_;
    struct Bar {
        Rect rect;
        std::size_t item;
    };
    std::vector<Bar> bars_;
    Axis valueAxis_;                                // as last painted
    std::vector<std::size_t> visible_;              // scratch: the rows that show
    std::optional<std::size_t> hovered_;
    std::optional<Point> mouse_;
    bool dragging_ = false;
    Point dragFrom_;
    double dragScroll_ = 0;
    // its scroll bar, every scrolling widget's (in rows): where it is, as last painted
    void placeBar();
    Rect barTrack_;
    ScrollBar bar_{Orientation::Vertical, [this](double rank, bool eased) { scrollTo(rank, eased); }, [this] { invalidatePaint(); }};
};

} // namespace glw
