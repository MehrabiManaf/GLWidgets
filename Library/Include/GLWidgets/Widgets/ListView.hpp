// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// A list of a model's items (ItemModel.hpp), as many as it holds: only the rows that show are widgets, made by a delegate
// and used again for other items as the list scrolls. Rows are all of one height (the theme's, or setRowHeight: any
// number of items costs nothing), or each as high as its widget measures (setRowHeight(0): measured when it shows,
// estimated until then; what shows stays in place as estimates are corrected).
//
// The current item is chosen by a click or the keys (arrows, Page Up / Down, Home, End) and, in Single mode, selected;
// a double click or Enter activates it. In Multiple mode, as a file browser's: a click selects an item alone, Ctrl+click
// (Cmd on macOS) adds or takes one, Shift+click selects from the anchor (the item last clicked alone or with Ctrl) to it,
// with Ctrl added to what was; the keys move as they do, Shift with them selecting from the anchor, Ctrl moving the
// current item only (Ctrl+Space then takes it or leaves it); Ctrl+A selects all. The selection is the model's items',
// kept as ranges (a million selected cost a range), and stays theirs as the model changes and the order does.
//
// It shows its items sorted (setSortOrder: the model's compare) and filtered (setFilter) as asked, without the model
// changing: its delegate, its handlers and what is asked of it speak of the model's items still (positions are what
// shows: shownCount, modelIndex). An item its model says is editable (ItemFlags::Editable) is edited in place, in a
// widget its delegate makes (makeEditor: a field, a check box, a combo box...), by a double click, F2 or typing.
//
//   auto texts = std::make_shared<glw::StringListModel>(std::vector<glw::LocalizedText>{"One", "Two"});
//   auto list = std::make_unique<glw::ListView>();
//   list->setModel(texts);                  // a StringListModel shows its texts by itself (a TextDelegate)
//   list->setOnActivated([](std::size_t i) { ... });
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/Animation.hpp>
#include <GLWidgets/Widgets/Bevel.hpp>
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
class ItemEditor;
class ItemOrder;
class IndexSet;
class Label;

// What makes the widgets of a list's rows, and shows an item in one.
class GLW_EXPORT ListDelegate {
public:
    virtual ~ListDelegate();
    // A row's widget: made when the list needs one more; it shows other items as the list scrolls.
    virtual std::unique_ptr<Widget> makeRow() = 0;
    // `row` (made by makeRow) shows the item at `index` now.
    virtual void bindRow(Widget& row, std::size_t index) = 0;
    // An editable item edited: the widget it is edited in, showing its value (any widget: a field, a check box, a combo
    // box...); none: it is not. Then what was edited goes into the model (Enter, the focus leaving it; not on Escape).
    virtual std::unique_ptr<Widget> makeEditor(std::size_t index) { return nullptr; }
    virtual void commitEdit(Widget& editor, std::size_t index) {}
};

// Each item's text in a row; edited, in a field (when told where the text goes).
class GLW_EXPORT TextDelegate : public ListDelegate {
public:
    explicit TextDelegate(std::function<LocalizedText(std::size_t)> text) : text_(std::move(text)) {}
    // What an editable item's text is set to, edited (none: it is not edited).
    void setOnEdited(std::function<void(std::size_t, std::string)> handler) { onEdited_ = std::move(handler); }
    std::unique_ptr<Widget> makeRow() override;
    void bindRow(Widget& row, std::size_t index) override;
    std::unique_ptr<Widget> makeEditor(std::size_t index) override;
    void commitEdit(Widget& editor, std::size_t index) override;

private:
    std::function<LocalizedText(std::size_t)> text_;
    std::function<void(std::size_t, std::string)> onEdited_;
};

// What starts editing an editable item (flags).
enum class EditTriggers : std::uint8_t {
    None = 0,
    DoubleClick = 1 << 0,   // a double click on it (otherwise it is activated)
    F2 = 1 << 1,            // F2 on the current one
    Typing = 1 << 2,        // typing on the current one: what is typed goes into its editor
    All = DoubleClick | F2 | Typing,
};
constexpr EditTriggers operator|(EditTriggers a, EditTriggers b) {
    return static_cast<EditTriggers>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}
constexpr bool hasTrigger(EditTriggers triggers, EditTriggers trigger) {
    return (static_cast<std::uint8_t>(triggers) & static_cast<std::uint8_t>(trigger)) != 0;
}

enum class SelectionMode : std::uint8_t {
    None,                   // a current item, none selected
    Single,                 // the current item is the one selected
    Multiple,               // any of them: by Ctrl and Shift with the mouse and the keys
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
    // Rows inserted where they show come out from under the row before them, those after them moving down with them, and
    // rows removed go back under it (the theme's normal motion): a tree's nodes opened and closed. Off by default.
    void setAnimatesChanges(bool animates) { animatesChanges_ = animates; }

    // The model's item that is current (none: none is, or it is filtered out).
    std::optional<std::size_t> current() const;
    // Scrolled to it, when it is one.
    void setCurrent(std::optional<std::size_t> index);

    // ---- the selection: the model's items ----
    bool isSelected(std::size_t index) const;
    // Those selected, in the model's order (Single: the current one).
    std::vector<std::size_t> selectedItems() const;
    std::size_t selectedCount() const;
    // In Multiple mode: one taken or left, all of those that show, none; those of `indices` only.
    void setSelected(std::size_t index, bool selected);
    void selectAll();
    void clearSelection();
    void setSelectedItems(const std::vector<std::size_t>& indices);
    void setOnSelectionChanged(std::function<void()> handler) { onSelectionChanged_ = std::move(handler); }

    // ---- sorting and filtering ----
    // Sorted by `compare` (the model's compare by default), ascending or descending; the model's own order among equals.
    // None: in the model's order.
    void setSortOrder(std::optional<SortOrder> order,
                      std::function<std::partial_ordering(std::size_t, std::size_t)> compare = {});
    std::optional<SortOrder> sortOrder() const { return sortOrder_; }
    // Only the items `accepts` says yes to (the model's indices); none: all of them.
    void setFilter(std::function<bool(std::size_t)> accepts);
    // Sorted and filtered again: what the comparison or the filter says changed (the text searched for). The model's
    // changes need not: the items they name only are compared and filtered (an item inserted goes where it sorts, one
    // changed stays where it is when it still sorts there), the rest of the order kept.
    void reorder();
    // How many show, which item shows at a position, where an item shows (none: filtered out).
    std::size_t shownCount() const;
    std::size_t modelIndex(std::size_t position) const;
    std::optional<std::size_t> position(std::size_t index) const;

    // ---- its header ----
    // A title over its items (none: no header, as by default); a click on it sorts them, ascending, then descending, then
    // not (as the model compares them, or as setSortOrder was told), an arrow in it saying which.
    void setHeader(LocalizedText title);
    const LocalizedText& header() const { return headerTitle_; }
    // Shown when it has a title (true by default); hidden at will.
    void setHeaderVisible(bool visible);
    bool headerVisible() const { return headerVisible_; }
    bool headerShown() const { return headerVisible_ && !headerTitle_.empty(); }
    // A click on the header sorts (true by default).
    void setSortingEnabled(bool enabled) { sortingEnabled_ = enabled; }

    // ---- editing ----
    void setEditTriggers(EditTriggers triggers) { editTriggers_ = triggers; }
    EditTriggers editTriggers() const { return editTriggers_; }
    // The item edited, if it is editable and its delegate makes an editor (true then).
    bool edit(std::size_t index);
    bool editing() const;
    std::optional<std::size_t> editedItem() const;
    Widget* editor() const;
    // What was edited put in / not.
    void commitEdit();
    void cancelEdit();
    // Where an item's editor goes in its row (the row's coordinates): all of it by default (a tree: after its arrow).
    void setEditorArea(std::function<Rect(const Widget& row)> area) { editorArea_ = std::move(area); }

    // Dips scrolled from the top (where it shows now), and how far it goes.
    double offset() const { return offset_; }
    // At once.
    void setOffset(double offset);
    // Eased there (the theme's scroll motion) from where it shows: what the wheel and a page do. Where it goes is a row's
    // top, the nearest (or the end), so that the rows at the top show whole.
    void scrollSmoothly(double offset, bool aimed = false);
    double maximumOffset() const;
    // Scrolled so that the item shows (as little as it takes).
    void scrollTo(std::size_t index);
    // The positions that show now: [first, last) (the model's items, unsorted and unfiltered).
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

    // Its own (the cells' borders shown, say), or none: the theme's itemView.
    void setStyle(std::optional<ItemViewStyle> style);
    const ItemViewStyle& style() const;

protected:
    Size measureContent(Size available) override;
    void arrangeContent(const Rect& content) override;
    void paint(PaintContext& context) override;
    void paintOver(PaintContext& context) override;
    Rect childrenClip() const override;
    std::optional<Rect> childClip(const Widget& child) const override;
    void onMouse(MouseEvent& event) override;
    void onPreviewKey(KeyEvent& event) override;
    void onKey(KeyEvent& event) override;
    void onText(TextEvent& event) override;
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
    void changesBegun() override;
    void changesEnded() override;

    void resetHeights();
    void show(double offset);           // the offset shown: clamped, the rows laid again
    double snapped(double offset) const;    // the nearest row's top, or the end
    // rows inserted or removed, easing in or out
    struct Change {
        std::size_t first = 0;          // inserted: the first of them; removed: the first after them (where they were)
        std::size_t count = 0;          // inserted: how many
        double height = 0;              // of all of them
        bool inserted = true;
        std::vector<std::pair<Widget*, double>> leaving;   // removed: their widgets, and where they were below the first's top
    };
    double gap() const;                 // how much of the change shows: what was inserted growing, what was removed shrinking
    double shiftOf(std::size_t index) const;    // a row after the change: how far from its place it shows
    Rect gapRect() const;               // where what was inserted or removed shows (its coordinates)
    void changeStepped();
    void endChange(bool lay = true);      // the rows laid again (not in the model's notifications: it has changed)
    // The rows that show for the offset: bound (the widgets of those that showed kept), measured, placed.
    void layRows();
    void activate(std::size_t position);
    void setCurrentAt(std::optional<std::size_t> position);
    bool selectedAt(std::size_t position) const;
    // Multiple: the items shown from position `a` to `b` (both) selected; what was, kept or not.
    void selectBetween(std::size_t a, std::size_t b, bool keep);
    // Multiple: what a click or a key at `position` does with Shift and Ctrl held.
    void chose(std::size_t position, bool shift, bool control);
    void selectionChanged();
    void scrollToPosition(std::size_t position);
    bool ordered() const { return sortOrder_.has_value() || static_cast<bool>(filter_); }
    // ---- the order, as the model changes: the items it names only ----
    // In the order shown, whether the model's `a` goes before its `b` (equals in the model's order).
    bool before(std::size_t a, std::size_t b) const;
    void buildOrder();                  // all of it, sorted and filtered
    // What a change of the order keeps: the current item, the item at the top and how far into it the view is.
    struct Kept {
        std::optional<std::size_t> current;     // the model's
        std::size_t currentAt = 0;              // where it was
        bool lost = false;                      // it went: the one in its place is current
        std::optional<std::size_t> top;
        double within = 0;
    };
    Kept keep() const;
    // After it: the positions [from, to) changed (`counted`: so did how many show).
    void reordered(Kept kept, std::size_t from, std::size_t to, bool counted);
    bool editAt(std::size_t position);
    void placeEditor();
    std::optional<Rect> editorRect() const;
    Rect scrollTrack() const;
    Rect scrollThumb() const;

    std::shared_ptr<ListModel> model_;
    std::unique_ptr<ListDelegate> delegate_;
    std::unique_ptr<RowHeights> heights_;
    std::optional<float> rowHeight_;
    SelectionMode selectionMode_ = SelectionMode::Single;
    std::optional<ItemViewStyle> style_;
    std::optional<std::size_t> current_;   // a position
    double offset_ = 0;
    std::optional<SortOrder> sortOrder_;
    std::function<std::partial_ordering(std::size_t, std::size_t)> compare_;
    std::function<bool(std::size_t)> filter_;
    std::unique_ptr<ItemOrder> order_;      // while sorted or filtered: the items shown, in their order
    std::unique_ptr<IndexSet> selection_;   // Multiple: the model's items selected
    // the style's motions: a press on a row ripples; the selection slides from the row it was on (where its pill showed:
    // its top in the rows, and its height) to the current one
    Ripple ripple_{[this] { invalidatePaint(); }};
    std::optional<std::size_t> rippled_;    // the position of the row pressed
    Animation slide_{[this] { invalidatePaint(); }};
    double slideFrom_ = 0, slideFromHeight_ = 0;
    // the selection's pill now: its top in the rows and its height (sliding, or on the current row)
    std::pair<double, double> selectionPill() const;
    std::optional<std::size_t> anchor_;     // ... where Shift selects from (the model's item)
    // a batch of the model's changes: the current item and the one at the top, by their handles (which stay theirs)
    struct Held {
        std::optional<std::uint32_t> current;
        std::size_t currentAt = 0;
        std::optional<std::uint32_t> top;
        double within = 0;
    };
    Held held_;
    int batch_ = 0;
    EditTriggers editTriggers_ = EditTriggers::All;
    std::unique_ptr<ItemEditor> editor_;
    std::optional<std::size_t> edited_;     // the position edited
    std::function<Rect(const Widget&)> editorArea_;
    bool animatesChanges_ = false;
    std::optional<Change> change_;
    std::vector<Widget*> revealed_;     // the rows cut to the gap now
    Animation changeMotion_{[this] { changeStepped(); }};
    Animation barWide_{[this] { invalidatePaint(); }};          // its bar's thumb: thin (0) or wide (1)
    SmoothScroll smooth_{[this](double offset) { show(offset); }};

    Rect area_;                         // its content rectangle, the bar's place in it
    Rect viewport_;                     // where rows show
    bool showBar_ = false;
    std::vector<Row> rows_;             // those that show, in order
    std::vector<Widget*> spare_;        // made, showing nothing
    bool rebindAll_ = false;            // what the rows show is out of date (the model changed)

    std::optional<std::size_t> hovered_;
    std::optional<std::size_t> pressed_;
    // the header
    LocalizedText headerTitle_;
    bool headerVisible_ = true;
    bool sortingEnabled_ = true;
    Label* headerLabel_ = nullptr;
    Rect headerRect_;
    bool headerHovered_ = false;
    bool headerPressed_ = false;
    bool barHovered_ = false;
    std::optional<double> thumbGrab_;   // the thumb dragged: where it was pressed, along it

    std::function<void(std::optional<std::size_t>)> onCurrentChanged_;
    std::function<void()> onSelectionChanged_;
    std::function<void(std::size_t)> onActivated_;
};

} // namespace glw
