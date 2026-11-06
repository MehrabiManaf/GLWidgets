// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// A view on a content larger than itself, scrolled by its scroll bars, the mouse wheel (with Shift: across), Page Up and
// Page Down, and to the widget the keyboard focus goes to. A content that does not scroll in a direction gets the
// viewport's length in it (a wrapped label is wrapped to the viewport's width).
//
// In right to left, the content starts on the right and the vertical bar is on the left; the horizontal offset is measured
// from the start side either way.
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/Animation.hpp>
#include <GLWidgets/Widgets/ScrollBar.hpp>
#include <GLWidgets/Widgets/Theme.hpp>
#include <GLWidgets/Widgets/Widget.hpp>

#include <cstdint>
#include <memory>
#include <optional>

namespace glw {

enum class ScrollBarPolicy : std::uint8_t {
    Auto,                   // a bar when the content is longer than the viewport
    Always,
    Never,                  // and no scrolling that way: the content gets the viewport's length
};

class GLW_EXPORT ScrollArea : public Widget {
public:
    // What a scroll area is made with (ui::ScrollArea): designated initializers, in this order.
    struct Props {
        ScrollBarPolicy horizontal = ScrollBarPolicy::Never;
        ScrollBarPolicy vertical = ScrollBarPolicy::Auto;
        std::optional<ScrollBarStyle> style;            // none: the theme's
    };

    ScrollArea();
    explicit ScrollArea(Props props);

    void setContent(std::unique_ptr<Widget> content);
    Widget* content() const { return content_; }
    void setStyle(std::optional<ScrollBarStyle> style);
    const ScrollBarStyle& style() const;

    // Dips scrolled: down, and from the start side across. Where it shows now (it may be easing to another).
    Point offset() const { return offset_; }
    // At once.
    void setOffset(Point offset);
    // Eased there (the theme's scroll motion) from where it shows: what the wheel, a page and revealing do.
    void scrollTo(Point offset);
    // ... from where it is going.
    void scrollBy(Point delta);
    Point maximumOffset() const;
    // Where the content shows, in its own coordinates (its content rectangle without the bars).
    Rect viewport() const { return viewport_; }

protected:
    Size measureContent(Size available) override;
    void arrangeContent(const Rect& content) override;
    void paintOver(PaintContext& context) override;
    void onMouse(MouseEvent& event) override;
    void onKey(KeyEvent& event) override;
    void onHoverChanged(bool hovered) override;
    Rect childrenClip() const override { return viewport_; }
    void revealArea(const Rect& area) override;

private:
    // Its bars where they are, as it is laid out and scrolled.
    void placeBars();
    void placeContent();
    void show(Point offset);                            // the offset shown: clamped, the content moved
    Point going() const;                                // where it is going

    Widget* content_ = nullptr;
    ScrollBarPolicy horizontal_ = ScrollBarPolicy::Never;
    ScrollBarPolicy vertical_ = ScrollBarPolicy::Auto;
    std::optional<ScrollBarStyle> style_;

    Point offset_;
    Rect viewport_;
    Size contentSize_;
    bool showVertical_ = false;
    bool showHorizontal_ = false;
    SmoothScroll smoothX_{[this](double x) { show({static_cast<float>(x), offset_.y}); }};
    SmoothScroll smoothY_{[this](double y) { show({offset_.x, static_cast<float>(y)}); }};
    ScrollBar verticalBar_{Orientation::Vertical,
                            [this](double y, bool eased) {
                                if (eased)
                                    scrollTo({going().x, static_cast<float>(y)});
                                else
                                    setOffset({offset_.x, static_cast<float>(y)});
                            },
                            [this] { invalidatePaint(); }};
    ScrollBar horizontalBar_{Orientation::Horizontal,
                              [this](double x, bool eased) {
                                  if (eased)
                                      scrollTo({static_cast<float>(x), going().y});
                                  else
                                      setOffset({static_cast<float>(x), offset_.y});
                              },
                              [this] { invalidatePaint(); }};
};

} // namespace glw
