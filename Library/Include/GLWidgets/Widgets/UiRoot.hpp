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
#include <GLWidgets/Widgets/Localization.hpp>
#include <GLWidgets/Widgets/Theme.hpp>
#include <GLWidgets/Widgets/Widget.hpp>

#include <memory>
#include <string>
#include <vector>

namespace glw::text {
class FontLibrary;
class GlyphAtlas;
} // namespace glw::text

namespace glw {

// What every root of an application shares.
struct UiContext {
    const text::FontLibrary& fonts;
    text::GlyphAtlas& atlas;
    const Translator& translator;
    Theme theme;
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

    Widget* focusWidget() const { return focus_; }
    void setFocus(Widget* widget);
    // The next (or previous) widget that can have the focus, in the order of the tree.
    void focusNext(bool backwards = false);

    // ---- frames ----

    bool needsLayout() const { return needsLayout_; }
    bool needsPaint() const { return needsPaint_; }
    // The theme / the language of the context changed: every widget is told, and the tree laid out again.
    void styleChanged();
    void languageChanged();
    void invalidateLayout();
    void invalidatePaint() { needsPaint_ = true; }
    void layout();
    // Records the whole tree into `list` (which it clears first).
    void paint(paint::DrawList& list);

private:
    friend class Widget;
    void forget(Widget& subtree);       // a subtree leaves: no pointer into it may stay
    std::vector<Widget*> pathTo(Widget* target) const;
    // Preview from the root down to `target`, then from it up: the widget that handled it, if one did.
    Widget* route(Widget* target, MouseEvent& event, Point point);
    void routeKey(KeyEvent& event);
    void routeText(TextEvent& event);
    void updateHover(Widget* target);
    void mouseMoved(Point point);
    void mouseButton(const platform::MouseButtonChanged& e);
    static void paintTree(Widget& widget, PaintContext& context);
    static Widget* hit(Widget& widget, Point point);     // `point` in the widget's parent's coordinates

    const UiContext& context_;
    UiHost* host_;
    std::unique_ptr<Widget> content_;
    Size size_;
    float scale_ = 1;
    bool rightToLeft_ = false;
    bool needsLayout_ = true;
    bool needsPaint_ = true;

    Widget* focus_ = nullptr;
    Widget* capture_ = nullptr;         // has the mouse while buttons are down
    int buttonsDown_ = 0;
    std::vector<Widget*> hovered_;      // the widget under the mouse and its ancestors, from the root down
    Point mouse_;
    platform::KeyModifiers modifiers_ = platform::KeyModifiers::None;
};

} // namespace glw
