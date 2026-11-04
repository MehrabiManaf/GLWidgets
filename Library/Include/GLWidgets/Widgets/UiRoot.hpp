// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// What shows a widget tree: it lays it out, paints it into a draw list and gives it the events of its window. It knows no
// platform window and no GPU: GLWidgets' Window (Application.hpp) drives one, and so may an engine, which gives it events
// (the plain structs of Platform/Event.hpp) and draws the lists it records.
#include <GLWidgets/Core/Geometry.hpp>
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Paint/DrawList.hpp>
#include <GLWidgets/Platform/Event.hpp>
#include <GLWidgets/Platform/Window.hpp>
#include <GLWidgets/Widgets/Animation.hpp>
#include <GLWidgets/Widgets/Localization.hpp>
#include <GLWidgets/Widgets/Theme.hpp>
#include <GLWidgets/Widgets/Widget.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace glw::text {
class FontLibrary;
class GlyphAtlas;
} // namespace glw::text

namespace glw {

class Action;
class DockingHost;
class IconAtlas;

// What every root of an application shares.
struct UiContext {
    const text::FontLibrary& fonts;
    text::GlyphAtlas& atlas;
    const Translator& translator;
    Theme theme;
    DockingHost* docking = nullptr;         // the windows, for docking across them; none: in its window only
    Scheduler* scheduler = nullptr;         // timers and animations; none: things happen at once, no tooltips
    IconAtlas* icons = nullptr;             // where SVG icons are rasterized; none: they draw nothing
};

class UiRoot;

// Where a popup opens, from what it opens from (its anchor).
enum class PopupPlacement : std::uint8_t {
    // Under it, their start sides aligned (a menu from a button, a list from a combo box); above it when there is more room
    // there.
    Below,
    // Beside it at its end side, their tops aligned (a submenu); at its start side when there is no room.
    After,
    // In its middle (a dialog, over the window it opens from).
    Center,
};

// A popup: a window of its own above the one it opens from, so that it may go past that window's edges.
struct PopupRequest {
    std::unique_ptr<Widget> content;        // as big as it measures
    Rect anchor;                            // in the root's dips: a button, a menu's item, the mouse (an empty rectangle)
    PopupPlacement placement = PopupPlacement::Below;
    bool tooltip = false;                   // takes no input (a menu or a list has the keyboard)
    // A dialog: the other windows take no input while it is open (it has the keyboard, and the popups opened from it), and
    // the one it opened from shows dimmed. It closes only by its host's closeWindow (its buttons, Escape), or with its
    // window.
    bool modal = false;
};

// What a root asks of the window it is in.
class GLW_EXPORT UiHost {
public:
    virtual ~UiHost();

    virtual platform::WindowState windowState() const = 0;
    virtual void minimizeWindow() = 0;
    virtual void toggleMaximizeWindow() = 0;
    virtual void closeWindow() = 0;
    // Typed text and the input method, while a text field has the focus; `area` (the root's dips) is where the text is
    // typed, for the input method to put its window by it.
    virtual void startTextInput() = 0;
    virtual void stopTextInput() = 0;
    virtual void setTextInputArea(const Rect& area) = 0;
    virtual std::string clipboardText() const = 0;
    virtual void setClipboardText(const std::string& text) = 0;
    // Opens a popup and gives its root, which the host keeps until it closes: by its own host's closeWindow, a click
    // outside it (in a window of the application: that click does nothing else), the application losing the focus, or
    // its window closing. Its widgets are told by onWindowClosing. A popup's host opens popups of its own (a submenu): they
    // close with it. None where there are no popups (a host of an engine, by default).
    virtual UiRoot* openPopup(PopupRequest request) { return nullptr; }
    // The pointer's shape over the window now (told as the mouse moves over it). Ignored by default (an engine's host).
    virtual void setCursor(platform::Cursor cursor) {}
};

class GLW_EXPORT UiRoot {
public:
    explicit UiRoot(const UiContext& context, UiHost* host = nullptr);
    ~UiRoot();
    UiRoot(const UiRoot&) = delete;
    UiRoot& operator=(const UiRoot&) = delete;

    const UiContext& context() const { return context_; }
    UiHost* host() const { return host_; }

    void setContent(std::unique_ptr<Widget> content);
    Widget* content() const { return content_.get(); }
    // The content's previous owner gets it back, the root shows nothing.
    std::unique_ptr<Widget> takeContent();

    void setSize(Size size);
    Size size() const { return size_; }
    void setScale(float pixelsPerDip);
    float scale() const { return scale_; }
    // The direction of everything whose direction is Inherit up to the root.
    void setLayoutDirection(LayoutDirection direction);
    bool rightToLeft() const { return rightToLeft_; }

    // ---- events (of its window; positions in the root's dips) ----

    void dispatch(const platform::Event& event);
    // What `point` is for a borderless window (Window::setHitTest).
    platform::HitArea windowHitTest(Point point) const;
    // The widget under `point`, the deepest one that can be under the mouse.
    Widget* widgetAt(Point point) const;
    // The pointer's shape now, as it was last told to the host.
    platform::Cursor cursor() const { return cursor_; }

    // Runs `call` once the event being dispatched is done (now when none is): what removes or moves widgets, the one
    // handling the event among them, does it from there.
    void defer(std::function<void()> call);

    // The mouse events go to `widget` until every button is released: a drag handed over from the widget pressed (which
    // may go while it goes on). Nothing when no button is down.
    void captureMouse(Widget& widget);

    // The actions whose shortcuts work in this root, those of `owner` (a menu bar, a toolbar, the application): a key that
    // the focused widget does not take triggers the first enabled one it is the shortcut of.
    void addShortcuts(const void* owner, std::vector<std::shared_ptr<Action>> actions);
    void removeShortcuts(const void* owner);

    Widget* focusWidget() const { return focus_; }
    void setFocus(Widget* widget);
    // The next (or previous) widget that can have the focus, in the order of the tree.
    void focusNext(bool backwards = false);

    // ---- frames ----

    bool needsLayout() const { return needsLayout_; }
    bool needsPaint() const { return needsPaint_; }
    // A color laid over everything it shows (a window under a dialog, dimmed); none: nothing.
    void setOverlay(std::optional<Color> color);
    // Its content this far from its place, drawn and under the mouse there: a popup sliding in.
    void setContentShift(Point shift);
    Point contentShift() const { return contentShift_; }
    // The theme / the language of the context changed: every widget is told, and the tree laid out again.
    void styleChanged();
    void languageChanged();
    // Its window is about to close: every widget is told, while the tree is whole.
    void windowClosing();
    // Everything laid out again: what every measure depends on changed (the theme, the language, the direction).
    void invalidateLayout();
    void invalidatePaint() { needsPaint_ = true; }
    // Changes with every invalidateLayout (of the root): a widget's measure for the same space, in the same generation and
    // not invalidated since (Widget::invalidateLayout), is kept.
    std::uint64_t layoutGeneration() const { return layoutGeneration_; }
    void layout();
    // Records the whole tree into `list` (which it clears first).
    void paint(paint::DrawList& list);

private:
    friend class Widget;
    void forget(Widget& subtree);       // a subtree leaves: no pointer into it may stay
    // A widget's layout changed (it marked itself and its ancestors): laid out at the next frame.
    void requestLayout() {
        needsLayout_ = true;
        needsPaint_ = true;
    }
    std::vector<Widget*> pathTo(Widget* target) const;
    // Preview from the root down to `target`, then from it up: the widget that handled it, if one did.
    Widget* route(Widget* target, MouseEvent& event, Point point);
    void routeKey(KeyEvent& event);
    void routeText(TextEvent& event);
    void updateHover(Widget* target);
    // The tooltip of what is under the mouse: shown after a delay, gone when the mouse leaves it or does something.
    void updateToolTip();
    void showToolTip();
    void hideToolTip(bool suppress);
    void mouseMoved(Point point);
    void runDeferred();                 // what was deferred, once nothing is dispatched
    // The pointer's shape: what has the mouse says, or the edge of the window under it, or what is under it.
    void updateCursor();
    void mouseButton(const platform::MouseButtonChanged& e);
    // `visible`: what of the widget's parent shows (its coordinates); a widget out of it is not painted, nor anything in it.
    static void paintTree(Widget& widget, PaintContext& context, const Rect& visible);
    static Widget* hit(Widget& widget, Point point);     // `point` in the widget's parent's coordinates

    const UiContext& context_;
    UiHost* host_;
    std::unique_ptr<Widget> content_;
    Size size_;
    float scale_ = 1;
    bool rightToLeft_ = false;
    bool needsLayout_ = true;
    bool needsPaint_ = true;
    std::optional<Color> overlay_;
    Point contentShift_;
    std::uint64_t layoutGeneration_ = 1;

    Widget* focus_ = nullptr;
    Widget* capture_ = nullptr;         // has the mouse while buttons are down
    int buttonsDown_ = 0;
    std::vector<Widget*> hovered_;      // the widget under the mouse and its ancestors, from the root down
    Point mouse_;
    platform::Cursor cursor_ = platform::Cursor::Arrow;
    platform::KeyModifiers modifiers_ = platform::KeyModifiers::None;
    int dispatching_ = 0;
    std::vector<std::function<void()>> deferred_;

    std::vector<std::pair<const void*, std::vector<std::shared_ptr<Action>>>> shortcuts_;

    Widget* tipFor_ = nullptr;          // the widget whose tooltip shows, or waits to
    UiRoot* tip_ = nullptr;             // the tooltip's popup
    Timer tipTimer_;
    bool tipSuppressed_ = false;        // the mouse did something over it: not again until it leaves
    std::optional<TimePoint> tipClosed_;
};

} // namespace glw
