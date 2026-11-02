// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// The widget: the unit of a user interface. Widgets form a tree in which a parent owns its children; a tree is shown by a
// UiRoot (UiRoot.hpp). New widgets derive from Widget, or from one of its subclasses, and override what they need: measure
// and arrange their content, paint, handle events.
//
// Layout runs in two passes over the tree: measure (the size a widget wants for the space offered, padding included,
// margin not) and arrange (the rectangle it gets, in its parent's coordinates). A Layout object (Layout.hpp) decides how
// children are placed; without one they are all placed over the widget's content.
//
// Directions: margin and padding have start and end sides, and alignments Start and End: left and right in left to right,
// right and left in right to left. A widget's layout direction is its parent's unless it is set, so a whole interface is
// mirrored by setting it once at the root.
//
// Events go to the widget under the mouse (or with the focus, for keys) in two phases: preview from the root down to it,
// then from it back up to the root. A handler marks an event handled to stop it. A widget handling a mouse press gets all
// mouse events until every button is released (it captures the mouse).
#include <GLWidgets/Core/Color.hpp>
#include <GLWidgets/Core/Geometry.hpp>
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Platform/Event.hpp>
#include <GLWidgets/Platform/Keys.hpp>
#include <GLWidgets/Platform/Window.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace glw::paint {
class Painter;
} // namespace glw::paint

namespace glw::text {
class TextLayout;
} // namespace glw::text

namespace glw {

class Layout;
class UiRoot;
struct UiContext;

inline constexpr float unbounded = std::numeric_limits<float>::infinity();

enum class LayoutDirection : std::uint8_t {
    Inherit,                // the parent's (the root's for the root's content)
    LeftToRight,
    RightToLeft,
};

// Where a widget goes in the space its layout gives it: all of it, or its desired size at the start, center or end.
enum class Alignment : std::uint8_t { Stretch, Start, Center, End };

// Space around a widget (margin) or inside it (padding). Start and end follow the layout direction.
struct Insets {
    float start = 0;
    float top = 0;
    float end = 0;
    float bottom = 0;

    static Insets all(float v) { return {v, v, v, v}; }
    static Insets symmetric(float horizontal, float vertical) { return {horizontal, vertical, horizontal, vertical}; }
    float horizontal() const { return start + end; }
    float vertical() const { return top + bottom; }
    // In physical sides: the rectangle `r` shrunk by these insets.
    Rect deflate(const Rect& r, bool rightToLeft) const {
        const float left = rightToLeft ? end : start, right = rightToLeft ? start : end;
        return {r.x + left, r.y + top, std::max(0.0f, r.width - left - right), std::max(0.0f, r.height - top - bottom)};
    }

    friend bool operator==(const Insets&, const Insets&) = default;
};

// ---- events ----

enum class MouseEventType : std::uint8_t { Press, Release, Move, Wheel };

struct MouseEvent {
    MouseEventType type = MouseEventType::Move;
    Point position;                                     // in the coordinates of the widget it is given to
    platform::MouseButton button = platform::MouseButton::Left;
    int clicks = 1;                                     // 2 for a double click
    Point wheelDelta;                                   // Wheel: steps, positive y away from the user
    platform::KeyModifiers modifiers = platform::KeyModifiers::None;
    bool handled = false;
};

struct KeyEvent {
    platform::Key key = platform::Key::Unknown;
    platform::KeyModifiers modifiers = platform::KeyModifiers::None;
    bool pressed = false;
    bool repeat = false;
    bool handled = false;
};

// Text typed, or composed by an input method (not typed yet: shown at the caret, replaced by the next one).
struct TextEvent {
    std::string text;                                   // UTF-8
    bool composing = false;
    int cursor = 0;                                     // composing: in code points
    bool handled = false;
};

// What a widget paints with: the painter (in the widget's coordinates) and what is shared by the interface.
struct GLW_EXPORT PaintContext {
    paint::Painter& painter;
    const UiContext& ui;

    void drawText(const text::TextLayout& layout, Point origin, Color color) const;
};

class GLW_EXPORT Widget {
public:
    Widget();
    virtual ~Widget();
    Widget(const Widget&) = delete;
    Widget& operator=(const Widget&) = delete;

    // ---- the tree ----

    Widget* parent() const { return parent_; }
    std::span<const std::unique_ptr<Widget>> children() const { return children_; }
    // Adds `child` (at `index`, at the end by default) and gives it back.
    Widget& addChild(std::unique_ptr<Widget> child, std::size_t index = std::numeric_limits<std::size_t>::max());
    template <class T>
    T& add(std::unique_ptr<T> child) {
        T& added = *child;
        addChild(std::move(child));
        return added;
    }
    // Takes `child` out of this widget: the caller owns it now (to put it elsewhere, or destroy it). A widget must not be
    // destroyed while one of its own event handlers runs.
    std::unique_ptr<Widget> removeChild(Widget& child);
    // The root showing this widget, if it is shown.
    UiRoot* root() const;

    // ---- geometry ----

    Rect bounds() const { return bounds_; }             // in the parent's coordinates, as arranged
    Size size() const { return bounds_.size(); }
    Point mapToRoot(Point local) const;
    Point mapFromRoot(Point point) const;

    // ---- layout ----

    void setLayout(std::unique_ptr<Layout> layout);
    Layout* layout() const { return layout_.get(); }

    void setMargin(Insets margin);
    Insets margin() const { return margin_; }
    void setPadding(Insets padding);
    Insets padding() const { return padding_; }
    void setMinimumSize(Size size);
    Size minimumSize() const { return minimumSize_; }
    void setMaximumSize(Size size);
    Size maximumSize() const { return maximumSize_; }
    // Shares of the space a stack layout has left: 0 takes none (the desired size), 2 twice what 1 takes.
    void setStretch(float stretch);
    float stretch() const { return stretch_; }
    void setAlignment(Alignment horizontal, Alignment vertical);
    Alignment horizontalAlignment() const { return horizontalAlignment_; }
    Alignment verticalAlignment() const { return verticalAlignment_; }

    void setLayoutDirection(LayoutDirection direction);
    LayoutDirection layoutDirection() const { return direction_; }
    bool rightToLeft() const;                           // the direction in effect

    // The size it wants within `available` (unbounded is infinity): padding included, margin not; between its minimum and
    // maximum sizes. Kept as desiredSize().
    Size measure(Size available);
    Size desiredSize() const { return desiredSize_; }
    // Gives it `rect` (in the parent's coordinates) and arranges its content.
    void arrange(const Rect& rect);

    // Something changed that its size or the place of what it shows depends on / that it shows.
    void invalidateLayout();
    void invalidatePaint();

    // ---- state ----

    void setVisible(bool visible);
    bool visible() const { return visible_; }
    // A disabled widget, and everything in it, gets no events and cannot have the focus.
    void setEnabled(bool enabled);
    bool enabled() const { return enabled_; }
    bool enabledInTree() const;
    void setFocusable(bool focusable) { focusable_ = focusable; }
    bool focusable() const { return focusable_; }
    // Not under the mouse itself (what is under it is its parent's), though its children may be: decorations, labels.
    void setHitTestVisible(bool visible) { hitTestVisible_ = visible; }
    bool hitTestVisible() const { return hitTestVisible_; }
    // Cuts its children to its bounds when they are painted.
    void setClipsChildren(bool clips);
    bool clipsChildren() const { return clipsChildren_; }
    // What it is for the window when it is under the mouse: Caption for what drags a borderless window.
    void setWindowArea(platform::HitArea area) { windowArea_ = area; }
    platform::HitArea windowArea() const { return windowArea_; }

    bool hasFocus() const;
    void focus();
    bool hovered() const { return hovered_; }           // the mouse is over it or one of its children

protected:
    // The size of its content for `available` (without padding): by default what its layout says, or the largest of its
    // children's (with their margins).
    virtual Size measureContent(Size available);
    // Places its content in `content` (its own coordinates, padding taken out): by default its layout does, or every child
    // gets all of it.
    virtual void arrangeContent(const Rect& content);

    // Paints it, in its own coordinates (0, 0 at its top left), before its children / after them.
    virtual void paint(PaintContext& context) {}
    virtual void paintOver(PaintContext& context) {}

    virtual void onPreviewMouse(MouseEvent& event) {}
    virtual void onMouse(MouseEvent& event) {}
    virtual void onPreviewKey(KeyEvent& event) {}
    virtual void onKey(KeyEvent& event) {}
    virtual void onText(TextEvent& event) {}
    virtual void onFocusChanged(bool focused) {}
    virtual void onHoverChanged(bool hovered) {}
    // It is now shown by another root, or by none: what depends on the root (fonts, scale) must be made again.
    virtual void onRootChanged() {}

    // For a widget filling a borderless window: what the point (its own coordinates) is for the window before the widget
    // under it is asked (WindowFrame: its resize borders).
    virtual std::optional<platform::HitArea> frameHitArea(Point point) const { return std::nullopt; }

    // The rectangle of its content in its own coordinates: its bounds at 0, 0 without padding.
    Rect contentRect() const;
    const UiContext* context() const;                   // the root's, when it is shown

private:
    friend class UiRoot;
    void setRootOfTree(UiRoot* root);
    void notifyRootChanged();

    Widget* parent_ = nullptr;
    UiRoot* uiRoot_ = nullptr;                          // set on the root's content only
    std::vector<std::unique_ptr<Widget>> children_;
    std::unique_ptr<Layout> layout_;

    Rect bounds_;
    Size desiredSize_;
    Insets margin_;
    Insets padding_;
    Size minimumSize_;
    Size maximumSize_{unbounded, unbounded};
    float stretch_ = 0;
    Alignment horizontalAlignment_ = Alignment::Stretch;
    Alignment verticalAlignment_ = Alignment::Stretch;
    LayoutDirection direction_ = LayoutDirection::Inherit;
    platform::HitArea windowArea_ = platform::HitArea::Client;

    bool visible_ = true;
    bool enabled_ = true;
    bool focusable_ = false;
    bool hitTestVisible_ = true;
    bool clipsChildren_ = false;
    bool hovered_ = false;
};

} // namespace glw
