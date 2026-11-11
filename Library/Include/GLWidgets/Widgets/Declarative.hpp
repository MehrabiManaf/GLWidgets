// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// Interfaces written as what they are: a tree of builders, each making a widget.
//
//   namespace ui = glw::ui;
//   struct Page {
//       glw::Label* status = nullptr;
//       std::unique_ptr<glw::Widget> build() {
//           return ui::VStack({.spacing = 16},
//                      ui::Label({.text = "Title", .fontSize = 26}),
//                      ui::HStack({.spacing = 8},
//                          ui::Button({.text = "OK", .onClick = [this] { status->setText("OK"); }}),
//                          ui::Button({.text = "Cancel"}).enabled(false)),
//                      ui::Label({.text = "Ready."}).bind(status))
//               .padding(glw::Insets::all(28));
//       }
//   };
//
// A builder takes the widget's Props (designated initializers, in the order the widget declares them) and, for containers,
// its children. What every widget has (stretch, margin, padding, alignment...) is set by the calls chained after it;
// anything else by with(). bind() gives back the widget made. A builder becomes a std::unique_ptr to its widget wherever one
// is expected: as a child, as a window's content. It makes ordinary widgets: built trees and widgets made one by one mix.
//
// Handlers run long after the tree is built: what they use must live as long. A bound pointer is a member of the class
// building the interface, and its handlers capture `this` (not a local variable of the function that builds the tree).
//
// The builders have the names of the widgets they make: write them qualified (ui::Label), or use `namespace glw::ui`
// without `namespace glw`, whose classes have the same names.
//
// The Props are plain aggregates on purpose: C++26 reflection will read them to load interfaces from files and to
// serialize them, without a line written for each widget.
#include <GLWidgets/Widgets/Button.hpp>
#include <GLWidgets/Widgets/CheckBox.hpp>
#include <GLWidgets/Widgets/CollapsiblePanel.hpp>
#include <GLWidgets/Widgets/ComboBox.hpp>
#include <GLWidgets/Widgets/Label.hpp>
#include <GLWidgets/Widgets/Layout.hpp>
#include <GLWidgets/Widgets/LineEdit.hpp>
#include <GLWidgets/Widgets/Panel.hpp>
#include <GLWidgets/Widgets/ScrollArea.hpp>
#include <GLWidgets/Widgets/SpinBox.hpp>
#include <GLWidgets/Widgets/Splitter.hpp>
#include <GLWidgets/Widgets/TabView.hpp>
#include <GLWidgets/Widgets/Widget.hpp>

#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace glw::ui {

// What becomes a child: a builder, or a widget already made.
template <class T>
concept Child = std::convertible_to<T, std::unique_ptr<Widget>>;

using Children = std::vector<std::unique_ptr<Widget>>;

// A widget being built. Each call returns the builder, so that calls chain.
template <class W>
class Element {
public:
    explicit Element(std::unique_ptr<W> widget) : widget_(std::move(widget)) {}

    Element stretch(float stretch) && { return apply([&](W& w) { w.setStretch(stretch); }); }
    Element toolTip(LocalizedText text) && { return apply([&](W& w) { w.setToolTip(std::move(text)); }); }
    Element toolTip(ToolTip tip) && { return apply([&](W& w) { w.setToolTip(std::move(tip)); }); }
    Element margin(Insets margin) && { return apply([&](W& w) { w.setMargin(margin); }); }
    Element padding(Insets padding) && { return apply([&](W& w) { w.setPadding(padding); }); }
    Element alignment(Alignment horizontal, Alignment vertical) && {
        return apply([&](W& w) { w.setAlignment(horizontal, vertical); });
    }
    Element minimumSize(Size size) && { return apply([&](W& w) { w.setMinimumSize(size); }); }
    Element maximumSize(Size size) && { return apply([&](W& w) { w.setMaximumSize(size); }); }
    Element direction(LayoutDirection direction) && { return apply([&](W& w) { w.setLayoutDirection(direction); }); }
    Element visible(bool visible) && { return apply([&](W& w) { w.setVisible(visible); }); }
    Element enabled(bool enabled) && { return apply([&](W& w) { w.setEnabled(enabled); }); }
    Element focusable(bool focusable) && { return apply([&](W& w) { w.setFocusable(focusable); }); }
    Element clipsChildren(bool clips) && { return apply([&](W& w) { w.setClipsChildren(clips); }); }
    // Anything else: `f` is given the widget.
    template <class F>
    Element with(F&& f) && {
        return apply(std::forward<F>(f));
    }
    // `pointer` is the widget made, for the code that changes it later. The tree owns it.
    template <class T>
        requires std::is_base_of_v<T, W>
    Element bind(T*& pointer) && {
        pointer = widget_.get();
        return std::move(*this);
    }

    template <class T>
        requires std::is_base_of_v<T, W>
    operator std::unique_ptr<T>() && {
        return std::move(widget_);
    }
    std::unique_ptr<W> build() && { return std::move(widget_); }

private:
    template <class F>
    Element apply(F&& f) {
        f(*widget_);
        return std::move(*this);
    }

    std::unique_ptr<W> widget_;
};

// Any widget: made with `arguments`.
template <class W, class... Arguments>
Element<W> make(Arguments&&... arguments) {
    return Element<W>(std::make_unique<W>(std::forward<Arguments>(arguments)...));
}

// `widget` with `children` added.
template <class W>
Element<W> withChildren(std::unique_ptr<W> widget, Children children) {
    for (auto& child : children)
        widget->addChild(std::move(child));
    return Element<W>(std::move(widget));
}

template <Child... C>
Children children(C&&... items) {
    Children list;
    list.reserve(sizeof...(C));
    (list.push_back(std::unique_ptr<Widget>(std::forward<C>(items))), ...);
    return list;
}

// ---- containers ----

struct StackProps {
    float spacing = 0;
};

inline Element<Widget> Stack(Orientation orientation, StackProps props, Children items) {
    auto widget = std::make_unique<Widget>();
    widget->setLayout(std::make_unique<StackLayout>(orientation, props.spacing));
    return withChildren(std::move(widget), std::move(items));
}

// Children top to bottom.
template <Child... C>
Element<Widget> VStack(StackProps props, C&&... items) {
    return Stack(Orientation::Vertical, props, children(std::forward<C>(items)...));
}
inline Element<Widget> VStack(StackProps props, Children items) {
    return Stack(Orientation::Vertical, props, std::move(items));
}

// Children from the start side to the end side (left to right, or right to left).
template <Child... C>
Element<Widget> HStack(StackProps props, C&&... items) {
    return Stack(Orientation::Horizontal, props, children(std::forward<C>(items)...));
}
inline Element<Widget> HStack(StackProps props, Children items) {
    return Stack(Orientation::Horizontal, props, std::move(items));
}

struct GridProps {
    std::vector<GridColumn> columns{{}, {.stretch = 1}};     // a form's: its labels, and its fields the rest
    float columnSpacing = 8;
    float rowSpacing = 6;
};

// Children in rows and columns, across the columns then on the next row (GridLayout). A child spanning columns: given to
// the grid's layout by with():
//   ui::Grid({}, ui::Label({.text = "Name"}), ui::LineEdit({}), ui::Label({.text = "..."}).bind(note))
//       .with([&](glw::Widget& grid) { static_cast<glw::GridLayout*>(grid.layout())->setSpan(*note, 2); })
inline Element<Widget> Grid(GridProps props, Children items) {
    auto widget = std::make_unique<Widget>();
    widget->setLayout(std::make_unique<GridLayout>(std::move(props.columns), props.columnSpacing, props.rowSpacing));
    return withChildren(std::move(widget), std::move(items));
}
template <Child... C>
Element<Widget> Grid(GridProps props, C&&... items) {
    return Grid(std::move(props), children(std::forward<C>(items)...));
}

// Children over each other, each placed by its alignments in the whole content.
template <Child... C>
Element<Widget> Overlay(C&&... items) {
    return withChildren(std::make_unique<Widget>(), children(std::forward<C>(items)...));
}

// A background, a border and rounded corners around its children.
template <Child... C>
Element<glw::Panel> Panel(glw::Panel::Props props, C&&... items) {
    return withChildren(std::make_unique<glw::Panel>(std::move(props)), children(std::forward<C>(items)...));
}

// The theme's card behind its children.
template <Child... C>
Element<glw::Card> Card(C&&... items) {
    return withChildren(std::make_unique<glw::Card>(), children(std::forward<C>(items)...));
}

// A view on `content`, scrolled.
template <Child C>
Element<glw::ScrollArea> ScrollArea(glw::ScrollArea::Props props, C&& content) {
    auto area = std::make_unique<glw::ScrollArea>(std::move(props));
    area->setContent(std::unique_ptr<Widget>(std::forward<C>(content)));
    return Element<glw::ScrollArea>(std::move(area));
}

// Panes with handles between them that the mouse drags; each pane's stretch is its share.
template <Child... C>
Element<glw::Splitter> Splitter(glw::Splitter::Props props, C&&... panes) {
    return withChildren(std::make_unique<glw::Splitter>(std::move(props)), children(std::forward<C>(panes)...));
}

// Nothing, taking `stretch` shares of what a stack has left: pushes what follows it to the end.
inline Element<Widget> Spacer(float stretch = 1) {
    return make<Widget>().stretch(stretch);
}

// ---- widgets ----

inline Element<glw::Label> Label(glw::Label::Props props) {
    return make<glw::Label>(std::move(props));
}

inline Element<glw::Button> Button(glw::Button::Props props) {
    return make<glw::Button>(std::move(props));
}

inline Element<glw::LineEdit> LineEdit(glw::LineEdit::Props props) {
    return make<glw::LineEdit>(std::move(props));
}

inline Element<glw::SpinBox> SpinBox(glw::SpinBox::Props props) {
    return make<glw::SpinBox>(std::move(props));
}

// Without tabs: added to it (TabView::addTab) once built.
inline Element<glw::TabView> TabView(glw::TabView::Props props) {
    return make<glw::TabView>(std::move(props));
}

inline Element<glw::CheckBox> CheckBox(glw::CheckBox::Props props) {
    return make<glw::CheckBox>(std::move(props));
}

// Its items go into its content.
template <class... C>
Element<glw::CollapsiblePanel> CollapsiblePanel(glw::CollapsiblePanel::Props props, C&&... items) {
    auto panel = std::make_unique<glw::CollapsiblePanel>(std::move(props));
    for (std::unique_ptr<Widget>& item : children(std::forward<C>(items)...))
        panel->content().addChild(std::move(item));
    return Element<glw::CollapsiblePanel>(std::move(panel));
}

inline Element<glw::ComboBox> ComboBox(glw::ComboBox::Props props) {
    return make<glw::ComboBox>(std::move(props));
}

} // namespace glw::ui
