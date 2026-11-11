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
// Only what changed is laid out again: invalidateLayout marks a widget and its ancestors; the others keep their measure
// (for the same space) and, given a rectangle of the same size, are only moved: what is in them stays where it is in them.
// Painting skips what is out of sight (out of the window, or of a scroll area's view).
//
// Directions: margin and padding have start and end sides, and alignments Start and End: left and right in left to right,
// right and left in right to left. A widget's layout direction is its parent's unless it is set, so a whole interface is
// mirrored by setting it once at the root.
//
// Events go to the widget under the mouse (or with the focus, for keys) in two phases: preview from the root down to it,
// then from it back up to the root. A handler marks an event handled to stop it. A widget handling a mouse press gets all
// mouse events until every button is released (it captures the mouse).
#include <GLWidgets/Widgets/DragDrop.hpp>
#include <GLWidgets/Core/Color.hpp>
#include <GLWidgets/Core/Geometry.hpp>
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Paint/Transform.hpp>
#include <GLWidgets/Platform/Event.hpp>
#include <GLWidgets/Platform/Keys.hpp>
#include <GLWidgets/Platform/Window.hpp>
#include <GLWidgets/Widgets/Localization.hpp>

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
struct ColorSpan;
} // namespace glw::text

namespace glw {

class Layout;
class UiRoot;
struct UiContext;
class Scheduler;

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
    bool wheelPrecise = false;                          // Wheel: by fractions of a step (a touchpad, a fine wheel)
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
    // ... in the colors of `spans` (text::ColorSpan), `color` out of them.
    void drawText(const text::TextLayout& layout, Point origin, Color color, std::span<const text::ColorSpan> spans) const;
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
    // `child` moved to `index` among its siblings (painted over those before it), staying in the tree: its focus, the
    // mouse captured by it are kept.
    void setChildIndex(Widget& child, std::size_t index);
    // The root showing this widget, if it is shown.
    UiRoot* root() const;

    // ---- geometry ----

    Rect bounds() const { return bounds_; }             // in the parent's coordinates, as arranged
    Size size() const { return bounds_.size(); }
    // Through the transforms of what it is in (a canvas's zoom and pan).
    Point mapToRoot(Point local) const;
    Point mapFromRoot(Point point) const;
    // Its coordinates to its root's.
    paint::Transform transformToRoot() const;

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
    // maximum sizes. Kept as desiredSize(), and not measured again for the same space until the layout is invalidated.
    Size measure(Size available);
    Size desiredSize() const { return desiredSize_; }
    // Gives it `rect` (in the parent's coordinates) and arranges its content: only moved, when it is the size it was and
    // nothing in it changed.
    void arrange(const Rect& rect);

    // Something changed that its size or the place of what it shows depends on / that it shows.
    void invalidateLayout();
    void invalidatePaint();

    // ---- state ----

    void setVisible(bool visible);
    bool visible() const { return visible_; }
    bool visibleInTree() const;                         // it and its ancestors
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

    // The pointer's shape over it, and over its children that have none: none by default, its parent's (the arrow at the
    // top). A widget whose parts differ (a splitter's handles) says so in cursorAt.
    void setCursor(std::optional<platform::Cursor> cursor) { cursor_ = cursor; }
    std::optional<platform::Cursor> cursor() const { return cursor_; }

    // What shows by the mouse when it stays over it a moment (UiRoot shows it, the theme's toolTip); none by default. A
    // widget whose parts differ (a node's pins, a list's rows) says so in toolTipAt.
    void setToolTip(LocalizedText text);
    const LocalizedText* toolTip() const { return toolTip_.get(); }

    bool hasFocus() const;
    void focus();
    bool hovered() const { return hovered_; }           // the mouse is over it or one of its children
    // Scrolls the scroll areas it is in so that it shows (as far as they can).
    void scrollIntoView();

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
    // The focus came into it or into one of its children / went out of all of them (an editor left: its value taken).
    virtual void onFocusWithinChanged(bool within) {}
    virtual void onHoverChanged(bool hovered) {}
    // It is now shown by another root, or by none: what depends on the root (fonts, theme, language) must be made again.
    virtual void onRootChanged() {}
    // The theme / the language of its root changed.
    virtual void onStyleChanged() {}
    virtual void onLanguageChanged() {}
    // It shows now, or does not: setVisible changed it or one of its ancestors.
    virtual void onVisibleChanged(bool visible) {}
    // The window showing it is about to close (UiRoot::windowClosing).
    virtual void onWindowClosing() {}

    // ---- drag and drop (DragDrop.hpp) ----
    // Something is dragged over it, at the event's position: what it would do with it if let go there (each time the mouse
    // moves; it may show where it would go). None: it does not take it, its parent is asked.
    virtual DropAction onDragOver(const DragEvent& event) { return DropAction::None; }
    // The drag it took is no longer over it: gone elsewhere, cancelled, or dropped (after onDrop).
    virtual void onDragLeave() {}
    // Let go over it, where onDragOver took it: what it did with it.
    virtual DropAction onDrop(const DragEvent& event) { return DropAction::None; }

    // For a widget filling a borderless window: what the point (its own coordinates) is for the window before the widget
    // under it is asked (WindowFrame: its resize borders).
    virtual std::optional<platform::HitArea> frameHitArea(Point point) const { return std::nullopt; }
    // The pointer's shape with the mouse at `point` (its own coordinates), over it or, while it has the mouse (a button
    // pressed on it), anywhere: setCursor's by default. None: its parent's.
    virtual std::optional<platform::Cursor> cursorAt(Point point) const { return cursor_; }
    // What shows by the mouse at `point` (its own coordinates) when it stays there a moment: setToolTip's by default. None:
    // its parent's. Asked as the mouse moves: another text shows in place of the one shown (another part).
    virtual std::optional<LocalizedText> toolTipAt(Point point) const {
        return toolTip_ ? std::optional<LocalizedText>(*toolTip_) : std::nullopt;
    }
    // Where its children are cut when it clips them (its own coordinates): all of it by default, a scroll area's viewport.
    // Outside of it, its children are neither painted nor under the mouse.
    virtual Rect childrenClip() const { return {0, 0, bounds_.width, bounds_.height}; }
    // Where one of its children is cut besides (its children's coordinates), neither painted nor under the mouse out of
    // it: rows of a list being revealed. None by default.
    virtual std::optional<Rect> childClip(const Widget& child) const { return std::nullopt; }
    // Its children's coordinates to its own (they are painted, under the mouse and mapped through it): none by default, a
    // canvas's zoom and pan.
    virtual paint::Transform childrenTransform() const { return {}; }
    // `area` (its own coordinates), something in it that must show: a scroll area scrolls to it.
    virtual void revealArea(const Rect& area) {}

    // The rectangle of its content in its own coordinates: its bounds at 0, 0 without padding.
    Rect contentRect() const;
    // Its content arranged again in the bounds it has (what arrangeContent depends on changed, its size did not).
    void rearrange();
    const UiContext* context() const;                   // the root's, when it is shown
    // The context's timers and animations, when it is shown (none: things happen at once).
    Scheduler* scheduler() const;
    // What `text` shows in the language of its root (its source text when it is not shown).
    std::string translate(const LocalizedText& text) const;

private:
    friend class UiRoot;
    void setRootOfTree(UiRoot* root);
    void notifyRootChanged();
    void notifyVisibleChanged(bool visible);

    Widget* parent_ = nullptr;
    UiRoot* uiRoot_ = nullptr;                          // set on the root's content only
    std::vector<std::unique_ptr<Widget>> children_;
    std::unique_ptr<Layout> layout_;

    Rect bounds_;
    Size desiredSize_;
    Size measuredFor_;                                  // the space desiredSize_ was measured for
    std::uint64_t measuredGeneration_ = 0;              // in its root's layout generation (0: not kept)
    std::uint64_t arrangedGeneration_ = 0;
    bool measureDirty_ = true;                          // it, or something in it, changed since it was measured
    bool arrangeDirty_ = true;                          // ... since it was arranged
    Insets margin_;
    Insets padding_;
    Size minimumSize_;
    Size maximumSize_{unbounded, unbounded};
    float stretch_ = 0;
    Alignment horizontalAlignment_ = Alignment::Stretch;
    Alignment verticalAlignment_ = Alignment::Stretch;
    LayoutDirection direction_ = LayoutDirection::Inherit;
    platform::HitArea windowArea_ = platform::HitArea::Client;
    std::optional<platform::Cursor> cursor_;

    bool visible_ = true;
    bool enabled_ = true;
    bool focusable_ = false;
    bool hitTestVisible_ = true;
    bool clipsChildren_ = false;
    bool hovered_ = false;
    std::unique_ptr<LocalizedText> toolTip_;            // most widgets have none: a pointer's worth
};

} // namespace glw
