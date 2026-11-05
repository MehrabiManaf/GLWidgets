// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/SearchPopup.hpp>

#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/ItemModel.hpp>
#include <GLWidgets/Widgets/Label.hpp>
#include <GLWidgets/Widgets/Layout.hpp>
#include <GLWidgets/Widgets/LineEdit.hpp>
#include <GLWidgets/Widgets/ListView.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>

namespace glw {

namespace {

// ASCII letters lowered (other scripts have no case to ignore, or are compared as they are).
std::string lowered(std::string text) {
    for (char& c : text) {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    }
    return text;
}

std::vector<std::string> words(const std::string& text) {
    std::vector<std::string> result;
    std::size_t at = 0;
    while (at < text.size()) {
        const std::size_t start = text.find_first_not_of(' ', at);
        if (start == std::string::npos)
            break;
        const std::size_t end = std::min(text.find(' ', start), text.size());
        result.push_back(text.substr(start, end - start));
        at = end;
    }
    return result;
}

// The items shown: indices into the options' items.
class Shown final : public ListModel {
public:
    std::vector<std::size_t> indices;
    std::size_t count() const override { return indices.size(); }
    using ListModel::notifyReset;
};

class SearchView;

// A row: the item's text, its detail muted at the end.
class RowDelegate final : public ListDelegate {
public:
    RowDelegate(const std::vector<SearchItem>& items, const Shown& shown) : items_(items), shown_(shown) {}

    std::unique_ptr<Widget> makeRow() override {
        auto row = std::make_unique<Widget>();
        row->setLayout(std::make_unique<StackLayout>(Orientation::Horizontal, 12.0f));
        row->setPadding({12, 0, 12, 0});
        auto& text = row->add(std::make_unique<Label>(Label::Props{.elide = true}));
        text.setAlignment(Alignment::Stretch, Alignment::Center);
        text.setStretch(1);
        auto& detail = row->add(std::make_unique<Label>(Label::Props{.fontSize = 12, .muted = true}));
        detail.setAlignment(Alignment::End, Alignment::Center);
        return row;
    }

    void bindRow(Widget& row, std::size_t index) override {
        const SearchItem& item = items_[shown_.indices[index]];
        static_cast<Label&>(*row.children()[0]).setText(item.text);
        static_cast<Label&>(*row.children()[1]).setText(item.detail);
    }

private:
    const std::vector<SearchItem>& items_;
    const Shown& shown_;
};

class SearchView final : public Widget {
public:
    explicit SearchView(SearchOptions options) : options_(std::move(options)), shown_(std::make_shared<Shown>()) {
        setLayout(std::make_unique<StackLayout>(Orientation::Vertical, 6.0f));
        setPadding(Insets::all(6));
        setMinimumSize(options_.size);
        setMaximumSize(options_.size);
        field_ = &add(std::make_unique<LineEdit>(LineEdit::Props{.text = options_.text,
                                                                 .placeholder = options_.placeholder,
                                                                 .onChanged = [this](const std::string&) { filter(); }}));
        list_ = &add(std::make_unique<ListView>());
        list_->setStretch(1);
        list_->setFocusable(false);         // the field keeps the focus: the keys choose for it
        list_->setModel(shown_);
        list_->setDelegate(std::make_unique<RowDelegate>(options_.items, *shown_));
        list_->setOnActivated([this](std::size_t index) { choose(index); });
    }

    void start() {
        filter();
        field_->focus();
    }

protected:
    void paint(PaintContext& context) override {
        const MenuStyle& s = context.ui.theme.menu;
        const Rect all{0, 0, size().width, size().height};
        context.painter.fillRect(all, s.background);
        context.painter.strokeRoundedRect(all, root() ? root()->windowRadius() : 0.0f, 1, s.border);
    }

    void onPreviewKey(KeyEvent& event) override {
        if (!event.pressed)
            return;
        const std::size_t count = shown_->indices.size();
        const std::size_t at = list_->current().value_or(0);
        const std::size_t page = std::max<std::size_t>(1, list_->shownItems().second - list_->shownItems().first);
        switch (event.key) {
        case platform::Key::Down: if (count) list_->setCurrent(std::min(at + 1, count - 1)); break;
        case platform::Key::Up: if (count) list_->setCurrent(at > 0 ? at - 1 : 0); break;
        case platform::Key::PageDown: if (count) list_->setCurrent(std::min(at + page, count - 1)); break;
        case platform::Key::PageUp: if (count) list_->setCurrent(at > page ? at - page : 0); break;
        case platform::Key::Enter:
            if (list_->current())
                choose(*list_->current());
            break;
        case platform::Key::Escape: close(); break;
        default: return;
        }
        event.handled = true;
    }

    void onPreviewMouse(MouseEvent& event) override {
        // a click on an item takes it
        if (event.type != MouseEventType::Release || event.button != platform::MouseButton::Left)
            return;
        const Point at = list_->mapFromRoot(mapToRoot(event.position));
        if (const std::optional<std::size_t> item = list_->itemAt(at)) {
            event.handled = true;
            choose(*item);
        }
    }

    void onWindowClosing() override {
        if (options_.onClosed)
            std::exchange(options_.onClosed, nullptr)();
    }

private:
    void filter() {
        std::vector<std::pair<std::string, std::string>> entries;
        entries.reserve(options_.items.size());
        for (const SearchItem& item : options_.items)
            entries.emplace_back(translate(item.text), translate(item.detail));
        shown_->indices = searchTexts(entries, field_->text());
        shown_->notifyReset();
        list_->setCurrent(shown_->indices.empty() ? std::nullopt : std::optional<std::size_t>(0));
    }

    void choose(std::size_t index) {
        if (index >= shown_->indices.size())
            return;
        const std::size_t item = shown_->indices[index];
        std::function<void(std::size_t)> chosen = options_.onChosen;
        close();
        if (chosen)
            chosen(item);
    }

    void close() {
        if (UiHost* host = root() ? root()->host() : nullptr)
            host->closeWindow();
    }

    SearchOptions options_;
    std::shared_ptr<Shown> shown_;
    LineEdit* field_ = nullptr;
    ListView* list_ = nullptr;
};

} // namespace

std::vector<std::size_t> searchTexts(const std::vector<std::pair<std::string, std::string>>& entries, const std::string& query) {
    const std::vector<std::string> wanted = words(lowered(query));
    std::vector<std::pair<int, std::size_t>> found;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const std::string text = lowered(entries[i].first);
        const std::string all = text + ' ' + lowered(entries[i].second);
        if (!std::all_of(wanted.begin(), wanted.end(), [&](const std::string& w) { return all.find(w) != std::string::npos; }))
            continue;
        // the first word at the start of the text, at the start of one of its words, or anywhere
        int rank = 0;
        if (!wanted.empty() && text.rfind(wanted.front(), 0) != 0)
            rank = text.find(' ' + wanted.front()) != std::string::npos ? 1 : 2;
        found.emplace_back(rank, i);
    }
    std::stable_sort(found.begin(), found.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    std::vector<std::size_t> result;
    result.reserve(found.size());
    for (const auto& f : found)
        result.push_back(f.second);
    return result;
}

bool showSearchPopup(Widget& from, const Rect& anchor, SearchOptions options) {
    UiRoot* root = from.root();
    UiHost* host = root ? root->host() : nullptr;
    if (!host)
        return false;
    UiRoot* opened = host->openPopup({.content = std::make_unique<SearchView>(std::move(options)), .anchor = from.transformToRoot().map(anchor)});
    if (!opened)
        return false;
    static_cast<SearchView*>(opened->content())->start();
    return true;
}

} // namespace glw
