// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// The scroll bar every scrolling widget has (ScrollArea, ListView and TreeView, TableView, TextEdit, a dock's tabs, a bar
// race): one look and one behavior wherever it is.
//
// Its look is the style's (ScrollBarStyle): its track, its thumb in its colors under the mouse and pressed, rounded as
// the style says, thin until the mouse is over the bar (or its thumb is dragged), then wide, eased (the theme's fast
// motion). In a strip (a dock's tabs) it may have no track. A vertical bar is at the end side of what it scrolls (on the
// left in right to left), its thumb against the outer edge; a horizontal one along the bottom, its thumb from the start
// side.
//
// What the mouse does to it: its thumb dragged follows the mouse (the offset at once); its track pressed beside the thumb
// pages toward where it was pressed, nine tenths of what shows, eased from where the scroll was going (pressed again on the
// way, a page further). A turn of the wheel goes through wheelScroll: a step from where the scroll was going, further as
// the wheel turns faster; by fractions of a step from a touchpad, as they come; and not taken when it moves nothing, so that
// what is around it scrolls instead.
//
// It is not a widget: a part of its widget, which places it as it is laid out or scrolled, paints it, and gives it its
// mouse events first.
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/Animation.hpp>
#include <GLWidgets/Widgets/Layout.hpp>
#include <GLWidgets/Widgets/Theme.hpp>
#include <GLWidgets/Widgets/Widget.hpp>

#include <functional>
#include <optional>

namespace glw {

class GLW_EXPORT ScrollBar {
public:
    // How far one page goes, of what shows.
    static constexpr double page = 0.9;

    // `scroll(offset, eased)`: its widget scrolled to `offset` (at once while the thumb is dragged; eased for a page);
    // `repaint`: its widget painted again (the bar changed under the mouse).
    ScrollBar(Orientation orientation, std::function<void(double offset, bool eased)> scroll, std::function<void()> repaint);
    ScrollBar(const ScrollBar&) = delete;
    ScrollBar& operator=(const ScrollBar&) = delete;

    // Where it is (its widget's coordinates) for a content `content` long in a view `view` long, scrolled `offset` (what
    // shows) on its way to `going`; `rightToLeft`: a horizontal one's offset from the right. Shown only when the content is
    // longer than the view.
    void place(const Rect& track, double content, double view, double offset, double going, bool rightToLeft,
               float minimumThumb = 24);
    void hide() { shown_ = false; }
    bool shown() const { return shown_; }
    Rect track() const { return shown_ ? track_ : Rect{}; }
    Rect thumb() const;
    bool dragging() const { return grab_.has_value(); }
    // The mouse over it (its track or its thumb).
    bool hovered() const { return hovered_ != Part::None; }

    // Its widget's mouse event, given to it first: true when it took it (pressed on it; its thumb dragged, let go).
    bool mouse(const MouseEvent& event);
    // The mouse gone from its widget.
    void leave();
    // Painted, as the style says; `track`: its track too (not in a strip).
    void paint(PaintContext& context, const ScrollBarStyle& style, bool track = true);

private:
    enum class Part : std::uint8_t { None, Track, Thumb };
    float along(Point p) const;             // from the track's start: its top, or its start side
    float thumbLength() const;
    float thumbAt() const;                  // along the track
    void setHovered(Part part);

    Orientation orientation_;
    std::function<void(double, bool)> scroll_;
    std::function<void()> repaint_;
    Rect track_;
    double content_ = 0, view_ = 0, offset_ = 0, going_ = 0;
    bool rightToLeft_ = false;
    float minimumThumb_ = 24;
    bool shown_ = false;
    Part hovered_ = Part::None;
    std::optional<float> grab_;             // its thumb dragged: where it was pressed, along it
    Animation wide_;
};

// Where a turn of the wheel sends a scroll that shows `shown` (on its way to smooth.target(shown)): `step` (a step of the
// wheel's worth, toward the end when positive) from where it was going, between 0 and `limit`. A wheel's step is boosted
// and aimed (SmoothScroll::aim: turned on and on, it goes evenly further); a touchpad's fractions (`precise`) as they come.
struct WheelScroll {
    double target = 0;
    bool aimed = false;             // what SmoothScroll::scrollTo takes
    bool moved = false;             // it goes somewhere else: the event taken
};
GLW_EXPORT WheelScroll wheelScroll(SmoothScroll& smooth, Scheduler* scheduler, double shown, double step, bool precise, double limit);

} // namespace glw
