// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/SearchPopup.hpp>

#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/ItemModel.hpp>
#include <GLWidgets/Widgets/Label.hpp>
#include <GLWidgets/Widgets/Layout.hpp>
#include <GLWidgets/Widgets/LineEdit.hpp>
#include <GLWidgets/Widgets/ListView.hpp>
#include <GLWidgets/Widgets/SearchField.hpp>
#include <GLWidgets/Widgets/TreeView.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <utility>

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

// The items shown in a tree of their folders (SearchOptions::tree): a node for each folder and each item, made again as
// the field is typed in.
class Folders final : public TreeModel {
public:
    // The items of `indices` (in this order) under their folders: a folder where its first item comes.
    void set(const std::vector<SearchItem>& items, const std::vector<std::size_t>& indices,
             const std::function<std::string(const LocalizedText&)>& translated) {
        nodes_.clear();
        top_.clear();
        std::map<std::pair<std::size_t, std::string>, std::size_t> folders;      // (parent + 1 or 0, name) -> node
        for (const std::size_t i : indices) {
            std::optional<std::size_t> parent;
            for (const LocalizedText& part : items[i].path) {
                const auto key = std::make_pair(parent ? *parent + 1 : 0, translated(part));
                auto it = folders.find(key);
                if (it == folders.end())
                    it = folders.emplace(key, add(Node{part, true, 0, parent})).first;
                parent = it->second;
            }
            add(Node{items[i].text, false, i, parent});
        }
        notifyReset();
    }
    // The item a node is (none: a folder).
    std::optional<std::size_t> itemOf(TreeNode node) const {
        const Node* n = at(node);
        return n && !n->folder ? std::optional<std::size_t>(n->item) : std::nullopt;
    }
    LocalizedText text(TreeNode node) const {
        const Node* n = at(node);
        return n ? n->text : LocalizedText();
    }

    std::size_t childCount(std::optional<TreeNode> parent) const override {
        if (!parent)
            return top_.size();
        const Node* n = at(*parent);
        return n ? n->children.size() : 0;
    }
    TreeNode child(std::optional<TreeNode> parent, std::size_t index) const override {
        return TreeNode{(parent ? at(*parent)->children[index] : top_[index]) + 1};
    }
    std::optional<TreeNode> parent(TreeNode node) const override {
        const Node* n = at(node);
        return n && n->parent ? std::optional<TreeNode>(TreeNode{*n->parent + 1}) : std::nullopt;
    }

private:
    struct Node {
        LocalizedText text;
        bool folder = false;
        std::size_t item = 0;
        std::optional<std::size_t> parent;
        std::vector<std::size_t> children{};
    };
    std::size_t add(Node node) {
        const std::size_t index = nodes_.size();
        (node.parent ? nodes_[*node.parent].children : top_).push_back(index);
        nodes_.push_back(std::move(node));
        return index;
    }
    const Node* at(TreeNode node) const { return node.id >= 1 && node.id <= nodes_.size() ? &nodes_[node.id - 1] : nullptr; }

    std::vector<Node> nodes_;
    std::vector<std::size_t> top_;
};

class SearchView final : public Widget {
public:
    explicit SearchView(SearchOptions options) : options_(std::move(options)), shown_(std::make_shared<Shown>()) {
        setLayout(std::make_unique<StackLayout>(Orientation::Vertical, 6.0f));
        setPadding(Insets::all(6));
        setMinimumSize(options_.size);
        setMaximumSize(options_.size);
        // a search field: its text cleared by its button (Escape closes the popup)
        field_ = &add(std::make_unique<SearchField>(SearchField::Props{.text = options_.text,
                                                                       .placeholder = options_.placeholder,
                                                                       .onChanged = [this](const std::string&) { filter(); }}))
                      .field();
        if (options_.tree) {
            // the items under their folders: the tree's rows are its list's
            folders_ = std::make_shared<Folders>();
            auto tree = std::make_unique<TreeView>();
            tree_ = tree.get();
            tree->setModel(folders_);
            tree->setDelegate(std::make_unique<TreeTextDelegate>([this](TreeNode node) { return folders_->text(node); }));
            tree->setHeaderVisible(false);
            tree->setStretch(1);
            list_ = &tree_->list();
            add(std::move(tree));
        } else {
            list_ = &add(std::make_unique<ListView>());
            list_->setStretch(1);
            list_->setModel(shown_);
            list_->setDelegate(std::make_unique<RowDelegate>(options_.items, *shown_));
        }
        list_->setFocusable(false);         // the field keeps the focus: the keys choose for it
        if (!tree_)
            list_->setOnActivated([this](std::size_t row) { choose(row); });
    }

    void start() {
        filter();
        if (options_.current && field_->text().empty() && *options_.current < options_.items.size()) {
            if (tree_) {
                // the item's folders opened to it
                for (std::size_t row = 0; row < tree_->shownCount(); ++row) {
                    if (folders_->itemOf(tree_->shownNode(row)) == *options_.current)
                        tree_->reveal(tree_->shownNode(row));
                }
            } else {
                list_->setCurrent(*options_.current);       // all of them, in their order: its row is its index
                list_->scrollTo(*options_.current);
            }
        }
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
        const std::size_t count = rowCount();
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
        if (list_->current())
            list_->scrollTo(*list_->current());
        event.handled = true;
    }

    void onPreviewMouse(MouseEvent& event) override {
        // pressed on a row, it has the mouse (not the list): a folder opens or closes (the tree's arrow left to this), an
        // item is taken when the mouse is let go on it (onMouse)
        if (event.button != platform::MouseButton::Left || event.type != MouseEventType::Press)
            return;
        const std::optional<std::size_t> row = rowAt(event.position);
        if (!row)
            return;
        event.handled = true;
        list_->setCurrent(*row);
        if (tree_ && !itemOfRow(*row))
            toggle(*row);
        else
            pressed_ = row;
    }

    void onMouse(MouseEvent& event) override {
        if (event.button != platform::MouseButton::Left || event.type != MouseEventType::Release)
            return;
        const std::optional<std::size_t> pressed = std::exchange(pressed_, std::nullopt);
        if (pressed && rowAt(event.position) == pressed) {
            event.handled = true;
            choose(*pressed);
        }
    }

    void onWindowClosing() override {
        if (options_.onClosed)
            std::exchange(options_.onClosed, nullptr)();
    }

private:
    std::size_t rowCount() const { return tree_ ? tree_->shownCount() : shown_->indices.size(); }
    // The row at `point` (its own coordinates); none over the list's scroll bar.
    std::optional<std::size_t> rowAt(Point point) const {
        const Point at = list_->mapFromRoot(mapToRoot(point));
        return list_->viewport().contains(at) ? list_->itemAt(at) : std::nullopt;
    }
    // The item a row shows (none: a folder).
    std::optional<std::size_t> itemOfRow(std::size_t row) const {
        if (tree_)
            return row < tree_->shownCount() ? folders_->itemOf(tree_->shownNode(row)) : std::nullopt;
        return row < shown_->indices.size() ? std::optional<std::size_t>(shown_->indices[row]) : std::nullopt;
    }

    void filter() {
        if (options_.search) {
            // the application's: what is not an item of it left out
            shown_->indices = options_.search(field_->text());
            std::erase_if(shown_->indices, [this](std::size_t i) { return i >= options_.items.size(); });
        } else {
            std::vector<std::pair<std::string, std::string>> entries;
            entries.reserve(options_.items.size());
            for (const SearchItem& item : options_.items)
                entries.emplace_back(translate(item.text), translate(item.detail));
            shown_->indices = searchTexts(entries, field_->text());
        }
        if (!tree_) {
            shown_->notifyReset();
            list_->setCurrent(shown_->indices.empty() ? std::nullopt : std::optional<std::size_t>(0));
            return;
        }
        // nothing typed: every item under its folders, the folders in order (closed, or open: `expanded`); found: under
        // their folders, open, each folder where its best item comes, the best one current
        const bool searching = !field_->text().empty();
        std::vector<std::size_t> order = shown_->indices;
        if (!searching) {
            const auto pathOf = [this](std::size_t i) {
                std::vector<std::string> parts;
                for (const LocalizedText& part : options_.items[i].path)
                    parts.push_back(translate(part));
                return parts;
            };
            std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return pathOf(a) < pathOf(b); });
        }
        folders_->set(options_.items, order, [this](const LocalizedText& text) { return translate(text); });
        if (searching || options_.expanded)
            tree_->expandAll();
        else
            tree_->collapseAll();
        std::optional<std::size_t> first;
        for (std::size_t row = 0; row < tree_->shownCount() && !first; ++row) {
            if (!searching || itemOfRow(row))
                first = row;
        }
        list_->setCurrent(first);
        if (first)
            list_->scrollTo(*first);
    }

    void toggle(std::size_t row) {
        const TreeNode node = tree_->shownNode(row);
        tree_->setExpanded(node, !tree_->isExpanded(node));
    }

    void choose(std::size_t row) {
        const std::optional<std::size_t> item = itemOfRow(row);
        if (!item) {
            if (tree_ && row < tree_->shownCount())
                toggle(row);            // a folder: opened or closed
            return;
        }
        std::function<void(std::size_t)> chosen = options_.onChosen;
        close();
        if (chosen)
            chosen(*item);
    }

    void close() {
        if (UiHost* host = root() ? root()->host() : nullptr)
            host->closeWindow();
    }

    SearchOptions options_;
    std::shared_ptr<Shown> shown_;
    std::shared_ptr<Folders> folders_;
    LineEdit* field_ = nullptr;
    ListView* list_ = nullptr;              // the list's, or the tree's rows
    TreeView* tree_ = nullptr;
    std::optional<std::size_t> pressed_;    // the row of an item pressed, taken when let go on it
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
