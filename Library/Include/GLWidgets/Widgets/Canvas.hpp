// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// A surface without edges, zoomed and panned, with widgets on it: the ground of a node editor, a diagram, a level map. Its
// items are any widgets, placed at points of the canvas, as big as they measure; they are painted, under the mouse and
// mapped through the zoom and the pan (Widget::childrenTransform), and text in them is rasterized at the size it shows, so
// it stays sharp at any zoom. Zooming and panning lay nothing out: only what shows is painted.
//
// The wheel zooms at the mouse; the middle or the right button drags the view (the right one, pressed and let go without
// moving, asks for a context menu: setOnContextRequested). Behind its items it draws a grid; a subclass draws what goes
// between the grid and the items, or over the items, in the canvas's coordinates (the links of a node editor, a
// selection rectangle): paintUnderItems, paintOverItems.
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/Theme.hpp>
#include <GLWidgets/Widgets/Widget.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace glw {

class GLW_EXPORT Canvas : public Widget {
public:
    Canvas();
    ~Canvas() override;

    // ---- items ----

    // `item` at `at` (the canvas's coordinates), over the items added before it.
    template <class T>
    T& addItem(std::unique_ptr<T> item, Point at) {
        T& added = *item;
        addChild(std::move(item));
        setItemPosition(added, at);
        return added;
    }
    // Taken off the canvas: the caller owns it now.
    std::unique_ptr<Widget> removeItem(Widget& item);
    void setItemPosition(Widget& item, Point at);
    Point itemPosition(const Widget& item) const;
    // Where an item is on the canvas (its position, the size it was laid out at).
    Rect itemRect(const Widget& item) const;
    // Where all items are; empty when there are none.
    Rect itemsBounds() const;
    // Over the others / under them.
    void raiseItem(Widget& item);
    void lowerItem(Widget& item);
    // The items that are, at least in part, in `area` of the canvas.
    std::vector<Widget*> itemsIn(const Rect& area) const;

    // ---- the view ----

    float zoom() const { return zoom_; }
    // `anchor` (its own coordinates) stays where it is: the mouse when the wheel zooms. The middle of it by default.
    void setZoom(float zoom, std::optional<Point> anchor = std::nullopt);
    void setZoomRange(float minimum, float maximum);
    float minimumZoom() const { return minimumZoom_; }
    float maximumZoom() const { return maximumZoom_; }
    // Where the canvas's origin is (its own coordinates).
    Point origin() const { return origin_; }
    void setOrigin(Point origin);
    // Panned so that the canvas's `point` is at its middle.
    void centerOn(Point point);
    // Zoomed (not past 1) and panned so that `area` of the canvas shows whole, `margin` dips around it.
    void frame(const Rect& area, float margin = 32);
    // All its items (when they are laid out, if they are not yet).
    void frameItems(float margin = 32);

    // Between the canvas's coordinates and its own.
    Point toCanvas(Point local) const { return viewTransform().unmap(local); }
    Point fromCanvas(Point point) const { return viewTransform().map(point); }
    paint::Transform viewTransform() const { return {zoom_, origin_}; }
    // What of the canvas shows.
    Rect visibleArea() const;

    void setGridVisible(bool visible);
    bool grid() const { return grid_; }
    // The right button let go where it was pressed: a menu for that point of the canvas, say.
    void setOnContextRequested(std::function<void(Point canvasPoint)> handler) { onContextRequested_ = std::move(handler); }
    // Zoomed or panned.
    void setOnViewChanged(std::function<void()> handler) { onViewChanged_ = std::move(handler); }

    const CanvasStyle& style() const;

protected:
    Size measureContent(Size available) override;
    void arrangeContent(const Rect& content) override;
    void paint(PaintContext& context) override;
    void paintOver(PaintContext& context) override;
    paint::Transform childrenTransform() const override { return viewTransform(); }
    std::optional<platform::Cursor> cursorAt(Point point) const override;
    void onMouse(MouseEvent& event) override;

    // In the canvas's coordinates, the painter transformed: over the grid, under the items / over them.
    virtual void paintUnderItems(PaintContext& context) {}
    virtual void paintOverItems(PaintContext& context) {}

private:
    void viewChanged();

    std::unordered_map<const Widget*, Point> positions_;
    float zoom_ = 1;
    float minimumZoom_ = 0.1f;
    float maximumZoom_ = 4;
    Point origin_;
    bool grid_ = true;
    std::optional<Rect> pendingFrame_;      // framed before it had a size: when it has one
    std::optional<float> pendingItemsFrame_;    // its items framed before they were laid out: the margin

    enum class Pan { None, Middle, Right };
    Pan panning_ = Pan::None;
    Point panFrom_;                         // where the mouse was, its own coordinates
    bool panMoved_ = false;
    std::function<void(Point)> onContextRequested_;
    std::function<void()> onViewChanged_;
};

} // namespace glw
