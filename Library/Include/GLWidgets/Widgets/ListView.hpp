// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// A list of a model's items (ItemModel.hpp), as many as it holds: only the rows that show are widgets, made by a delegate
// and used again for other items as the list scrolls. Rows are all of one height (the theme's, or setRowHeight: any
// number of items costs nothing), or each as high as its widget measures (setRowHeight(0): measured when it shows,
// estimated until then; what shows stays in place as estimates are corrected).
//
// The current item is chosen by a click or the keys (arrows, Page Up / Down, Home, End) and, in Single mode, selected;
// a double click or Enter activates it.
//
//   auto texts = std::make_shared<glw::StringListModel>(std::vector<glw::LocalizedText>{"One", "Two"});
//   auto list = std::make_unique<glw::ListView>();
//   list->setModel(texts);                  // a StringListModel shows its texts by itself (a TextDelegate)
//   list->setOnActivated([](std::size_t i) { ... });
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/ItemModel.hpp>
#include <GLWidgets/Widgets/Theme.hpp>
#include <GLWidgets/Widgets/Widget.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace glw {

class RowHeights;

// What makes the widgets of a list's rows, and shows an item in one.
class GLW_EXPORT ListDelegate {
public:
    virtual ~ListDelegate();
    // A row's widget: made when the list needs one more; it shows other items as the list scrolls.
    virtual std::unique_ptr<Widget> makeRow() = 0;
    // `row` (made by makeRow) shows the item at `index` now.
    virtual void bindRow(Widget& row, std::size_t index) = 0;
};

// Each item's text in a row.
class GLW_EXPORT TextDelegate : public ListDelegate {
public:
    explicit TextDelegate(std::function<LocalizedText(std::size_t)> text) : text_(std::move(text)) {}
    std::unique_ptr<Widget> makeRow() override;
    void bindRow(Widget& row, std::size_t index) override;

private:
    std::function<LocalizedText(std::size_t)> text_;
};

enum class SelectionMode : std::uint8_t {
    None,                   // a current item, none selected
    Single,                 // the current item is the one selected
};

class GLW_EXPORT ListView : public Widget, private ListModelObserver {
public:
    ListView();
    ~ListView() override;

    // A StringListModel without a delegate is shown by a TextDelegate.
    void setModel(std::shared_ptr<ListModel> model);
    ListModel* model() const { return model_.get(); }
    void setDelegate(std::unique_ptr<ListDelegate> delegate);
    // Every row this high (none: the theme's); 0: each as high as its widget measures.
    void setRowHeight(std::optional<float> height);
    void setSelectionMode(SelectionMode mode);
    SelectionMode selectionMode() const { return selectionMode_; }

    std::optional<std::size_t> current() const { return current_; }
    // Scrolled to it, when it is one.
    void setCurrent(std::optional<std::size_t> index);
    bool isSelected(std::size_t index) const { return selectionMode_ == SelectionMode::Single && current_ == index; }

    // Dips scrolled from the top, and how far it goes.
    double offset() const { return offset_; }
    void setOffset(double offset);
    double maximumOffset() const;
    // Scrolled so that the item shows (as little as it takes).
    void scrollTo(std::size_t index);
    // The items that show now: [first, last).
    std::pair<std::size_t, std::size_t> shownItems() const;
    // The widget showing the item, while it shows.
    Widget* rowWidget(std::size_t index) const;
    // The item at `point` (its coordinates), if one is there.
    std::optional<std::size_t> itemAt(Point point) const;
    // Where the item is (its coordinates), while it shows.
    std::optional<Rect> itemRect(std::size_t index) const;
    // Where its rows show (its coordinates): it less its scroll bar.
    Rect viewport() const { return viewport_; }

    void setOnCurrentChanged(std::function<void(std::optional<std::size_t>)> handler) { onCurrentChanged_ = std::move(handler); }
    // A double click or Enter on an item.
    void setOnActivated(std::function<void(std::size_t)> handler) { onActivated_ = std::move(handler); }

    const ItemViewStyle& style() const;

protected:
    Size measureContent(Size available) override;
    void arrangeContent(const Rect& content) override;
    void paint(PaintContext& context) override;
    void paintOver(PaintContext& context) override;
    Rect childrenClip() const override { return viewport_; }
    void onMouse(MouseEvent& event) override;
    void onKey(KeyEvent& event) override;
    void onHoverChanged(bool hovered) override;
    void onFocusChanged(bool focused) override;
    void onRootChanged() override;
    void onStyleChanged() override;

private:
    struct Row {
        Widget* widget = nullptr;
        std::size_t index = 0;
    };
    // the model's changes
    void itemsInserted(std::size_t first, std::size_t count) override;
    void itemsRemoved(std::size_t first, std::size_t count) override;
    void itemsChanged(std::size_t first, std::size_t count) override;
    void modelReset() override;

    void resetHeights();
    // The rows that show for the offset: bound (the widgets of those that showed kept), measured, placed.
    void layRows();
    void activate(std::size_t index);
    Rect scrollTrack() const;
    Rect scrollThumb() const;

    std::shared_ptr<ListModel> model_;
    std::unique_ptr<ListDelegate> delegate_;
    std::unique_ptr<RowHeights> heights_;
    std::optional<float> rowHeight_;
    SelectionMode selectionMode_ = SelectionMode::Single;
    std::optional<std::size_t> current_;
    double offset_ = 0;

    Rect area_;                         // its content rectangle, the bar's place in it
    Rect viewport_;                     // where rows show
    bool showBar_ = false;
    std::vector<Row> rows_;             // those that show, in order
    std::vector<Widget*> spare_;        // made, showing nothing
    bool rebindAll_ = false;            // what the rows show is out of date (the model changed)

    std::optional<std::size_t> hovered_;
    std::optional<std::size_t> pressed_;
    bool barHovered_ = false;
    std::optional<double> thumbGrab_;   // the thumb dragged: where it was pressed, along it

    std::function<void(std::optional<std::size_t>)> onCurrentChanged_;
    std::function<void(std::size_t)> onActivated_;
};

} // namespace glw
