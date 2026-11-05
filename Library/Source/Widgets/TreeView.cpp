// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/TreeView.hpp>

#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/Label.hpp>
#include <GLWidgets/Widgets/Layout.hpp>
#include <GLWidgets/Widgets/LineEdit.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <cmath>

namespace glw {

namespace {

constexpr float arrowWidth = 16;        // the room of a row's arrow, after its indent

const ItemViewStyle& itemStyle(const Widget& widget) {
    const UiRoot* r = widget.root();
    if (r)
        return r->context().theme.itemView;
    static const ItemViewStyle unstyled = Theme::dark().itemView;
    return unstyled;
}

// A row of a tree: its indent, its arrow (by a node with children), the delegate's widget after them.
class TreeRow final : public Widget {
public:
    TreeRow(std::unique_ptr<Widget> content, std::function<void(std::size_t)> toggle, std::function<float(TreeNode, bool)> turn)
        : toggle_(std::move(toggle)), turn_(std::move(turn)) {
        content_ = &addChild(std::move(content));
    }

    Widget& content() const { return *content_; }

    void show(std::size_t index, TreeNode node, std::uint32_t depth, bool expandable, bool expanded) {
        index_ = index;
        if (depth != depth_) {
            depth_ = depth;
            invalidateLayout();
        }
        node_ = node;
        expandable_ = expandable;
        expanded_ = expanded;
        invalidatePaint();
    }

protected:
    Size measureContent(Size available) override {
        const float lead = this->lead();
        const Size s = content_->measure({std::max(0.0f, available.width - lead), available.height});
        return {s.width + lead, s.height};
    }

    void arrangeContent(const Rect& content) override {
        const float lead = std::min(this->lead(), content.width);
        const bool rtl = rightToLeft();
        arrangeInCell(*content_, {rtl ? content.x : content.x + lead, content.y, content.width - lead, content.height}, rtl);
    }

    void paint(PaintContext& context) override {
        if (!expandable_)
            return;
        // a chevron: toward the end side when collapsed, down when expanded, turning between
        const Rect a = arrow();
        const Point c = a.center();
        const bool rtl = rightToLeft();
        const float d = rtl ? -1.0f : 1.0f;
        const Color color = itemStyle(*this).expander;
        const float angle = turn_(node_, expanded_) * 1.5707964f * d;
        const float cs = std::cos(angle), sn = std::sin(angle);
        Point points[] = {{-2 * d, -4}, {2 * d, 0}, {-2 * d, 4}};
        for (Point& q : points)
            q = {c.x + q.x * cs - q.y * sn, c.y + q.x * sn + q.y * cs};
        context.painter.drawPolyline(points, 1.4f, color);
    }

    void onMouse(MouseEvent& event) override {
        // the arrow pressed: expanded or collapsed (the row is not chosen for it)
        if (event.type != MouseEventType::Press || event.button != platform::MouseButton::Left || !expandable_ ||
            !arrow().contains(event.position))
            return;
        event.handled = true;
        toggle_(index_);
    }

private:
    float lead() const { return itemStyle(*this).indent * static_cast<float>(depth_) + arrowWidth; }

    Rect arrow() const {
        const float x = itemStyle(*this).indent * static_cast<float>(depth_);
        return rightToLeft() ? Rect{size().width - x - arrowWidth, 0, arrowWidth, size().height} : Rect{x, 0, arrowWidth, size().height};
    }

    std::function<void(std::size_t)> toggle_;
    Widget* content_ = nullptr;
    std::size_t index_ = 0;
    std::uint32_t depth_ = 0;
    bool expandable_ = false;
    bool expanded_ = false;
    TreeNode node_;
    std::function<float(TreeNode, bool)> turn_;         // how far its node's arrow has turned, from 0 to 1
};

} // namespace

TreeDelegate::~TreeDelegate() = default;

std::unique_ptr<Widget> TreeTextDelegate::makeRow() {
    auto row = std::make_unique<Widget>();
    row->setPadding({4, 0, 12, 0});
    auto& label = row->add(std::make_unique<Label>(Label::Props{.elide = true}));
    label.setAlignment(Alignment::Start, Alignment::Center);
    return row;
}

void TreeTextDelegate::bindRow(Widget& row, TreeNode node) {
    static_cast<Label&>(*row.children()[0]).setText(text_(node));
}

std::unique_ptr<Widget> TreeTextDelegate::makeEditor(TreeNode node) {
    if (!onEdited_)
        return nullptr;
    auto field = std::make_unique<LineEdit>(LineEdit::Props{.text = text_(node).source});
    field->selectAll();
    return field;
}

void TreeTextDelegate::commitEdit(Widget& editor, TreeNode node) {
    if (onEdited_)
        onEdited_(node, static_cast<LineEdit&>(editor).text());
}

// ---- the list's model and delegate ----

class TreeView::Rows final : public ListModel {
public:
    explicit Rows(const TreeView& tree) : tree_(tree) {}
    std::vector<Entry> entries;

    std::size_t count() const override { return entries.size(); }
    ItemFlags flags(std::size_t index) const override {
        return tree_.model_ && tree_.delegate_ ? tree_.model_->flags(entries[index].node) : ItemFlags::None;
    }
    using ListModel::notifyChanged;
    using ListModel::notifyInserted;
    using ListModel::notifyRemoved;
    using ListModel::notifyReset;

private:
    const TreeView& tree_;
};

class TreeView::RowDelegate final : public ListDelegate {
public:
    explicit RowDelegate(TreeView& tree) : tree_(tree) {}

    std::unique_ptr<Widget> makeRow() override {
        std::unique_ptr<Widget> content = tree_.delegate_ ? tree_.delegate_->makeRow() : std::make_unique<Widget>();
        return std::make_unique<TreeRow>(std::move(content), [tree = &tree_](std::size_t index) { tree->toggle(index); },
                                         [tree = &tree_](TreeNode node, bool expanded) { return tree->turnOf(node, expanded); });
    }

    void bindRow(Widget& row, std::size_t index) override {
        auto& treeRow = static_cast<TreeRow&>(row);
        const Entry& entry = tree_.rows_->entries[index];
        treeRow.show(index, entry.node, entry.depth, entry.expandable, entry.expanded);
        if (tree_.delegate_)
            tree_.delegate_->bindRow(treeRow.content(), entry.node);
    }

    std::unique_ptr<Widget> makeEditor(std::size_t index) override {
        return tree_.delegate_ ? tree_.delegate_->makeEditor(tree_.rows_->entries[index].node) : nullptr;
    }
    void commitEdit(Widget& editor, std::size_t index) override {
        if (tree_.delegate_ && index < tree_.rows_->entries.size())
            tree_.delegate_->commitEdit(editor, tree_.rows_->entries[index].node);
    }

private:
    TreeView& tree_;
};

// ---- the view ----

TreeView::TreeView() : rows_(std::make_shared<Rows>(*this)) {
    list_ = &add(std::make_unique<ListView>());
    list_->setModel(rows_);
    list_->setDelegate(std::make_unique<RowDelegate>(*this));
    list_->setAnimatesChanges(true);        // a node's children come out from under it, and go back
    // an editor after the indent and the arrow
    list_->setEditorArea([](const Widget& row) { return static_cast<const TreeRow&>(row).content().bounds(); });
    list_->setOnCurrentChanged([this](std::optional<std::size_t> index) {
        if (onCurrentChanged_)
            onCurrentChanged_(index ? std::optional<TreeNode>(rows_->entries[*index].node) : std::nullopt);
    });
    list_->setOnActivated([this](std::size_t index) {
        const TreeNode node = rows_->entries[index].node;
        if (rows_->entries[index].expandable)
            toggle(index);
        if (onActivated_)
            onActivated_(node);
    });
}

TreeView::~TreeView() {
    if (model_)
        model_->removeObserver(*this);
}

void TreeView::setModel(std::shared_ptr<TreeModel> model) {
    if (model_)
        model_->removeObserver(*this);
    model_ = std::move(model);
    if (model_)
        model_->addObserver(*this);
    expanded_.clear();
    rebuild();
}

void TreeView::setDelegate(std::unique_ptr<TreeDelegate> delegate) {
    delegate_ = std::move(delegate);
    list_->setDelegate(std::make_unique<RowDelegate>(*this));       // its rows made again, with the new delegate's
}

void TreeView::setRowHeight(std::optional<float> height) {
    list_->setRowHeight(height);
}

std::size_t TreeView::shownCount() const {
    return rows_->entries.size();
}

TreeNode TreeView::shownNode(std::size_t index) const {
    return rows_->entries[index].node;
}

std::uint32_t TreeView::depthOf(std::size_t index) const {
    return rows_->entries[index].depth;
}

std::optional<std::size_t> TreeView::shownIndexOf(TreeNode node) const {
    const auto& entries = rows_->entries;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].node == node)
            return i;
    }
    return std::nullopt;
}

std::optional<TreeNode> TreeView::current() const {
    const std::optional<std::size_t> index = list_->current();
    return index ? std::optional<TreeNode>(rows_->entries[*index].node) : std::nullopt;
}

void TreeView::setCurrent(TreeNode node) {
    if (const std::optional<std::size_t> index = shownIndexOf(node))
        list_->setCurrent(*index);
}

// ---- the entries ----

std::vector<TreeNode> TreeView::shownChildren(std::optional<TreeNode> parent) const {
    const std::size_t count = model_->childCount(parent);
    std::vector<TreeNode> children;
    children.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const TreeNode node = model_->child(parent, i);
        if (!filter_ || kept_.contains(node.id))
            children.push_back(node);
    }
    if (sortOrder_) {
        const bool descending = *sortOrder_ == SortOrder::Descending;
        std::stable_sort(children.begin(), children.end(), [&](TreeNode a, TreeNode b) {
            const std::partial_ordering o = model_->compare(a, b);
            return descending ? o > 0 : o < 0;
        });
    }
    return children;
}

bool TreeView::expandedNow(TreeNode node) const {
    return expanded_.contains(node.id) || opened_.contains(node.id);
}

void TreeView::appendChildren(std::optional<TreeNode> parent, std::uint32_t depth, std::vector<Entry>& out) const {
    for (const TreeNode node : shownChildren(parent)) {
        // filtered: expandable only by what it keeps in it
        const bool expandable = model_->hasChildren(node);
        const bool expanded = expandable && expandedNow(node);
        out.push_back({node, depth, expandable, expanded});
        if (expanded)
            appendChildren(node, depth + 1, out);
    }
}

void TreeView::setSortOrder(std::optional<SortOrder> order) {
    sortOrder_ = order;
    rebuild();
}

void TreeView::setFilter(std::function<bool(TreeNode)> accepts) {
    filter_ = std::move(accepts);
    reorder();
}

void TreeView::reorder() {
    refilter();
    rebuild();
}

void TreeView::refilter() {
    kept_.clear();
    opened_.clear();
    if (!filter_ || !model_)
        return;
    // every node, from the bottom up: kept when accepted or when one of its children is (then opened, to show it)
    const auto visit = [&](auto& self, TreeNode node) -> bool {
        bool keepsAChild = false;
        if (model_->hasChildren(node)) {
            const std::size_t count = model_->childCount(node);
            for (std::size_t i = 0; i < count; ++i)
                keepsAChild = self(self, model_->child(node, i)) || keepsAChild;
        }
        if (keepsAChild)
            opened_.insert(node.id);
        const bool kept = keepsAChild || filter_(node);
        if (kept)
            kept_.insert(node.id);
        return kept;
    };
    const std::size_t count = model_->childCount(std::nullopt);
    for (std::size_t i = 0; i < count; ++i)
        visit(visit, model_->child(std::nullopt, i));
}

bool TreeView::edit(TreeNode node) {
    const std::optional<std::size_t> index = shownIndexOf(node);
    return index && list_->edit(*index);
}

void TreeView::rebuild() {
    list_->cancelEdit();
    rows_->entries.clear();
    if (model_)
        appendChildren(std::nullopt, 0, rows_->entries);
    rows_->notifyReset();
}

std::size_t TreeView::subtreeEnd(std::size_t index) const {
    const auto& entries = rows_->entries;
    std::size_t end = index + 1;
    while (end < entries.size() && entries[end].depth > entries[index].depth)
        ++end;
    return end;
}

std::optional<std::size_t> TreeView::childPosition(std::optional<TreeNode> parent, std::size_t child) const {
    std::size_t at = 0;
    if (parent) {
        const std::optional<std::size_t> index = shownIndexOf(*parent);
        if (!index || !rows_->entries[*index].expanded)
            return std::nullopt;
        at = *index + 1;
    }
    for (std::size_t k = 0; k < child && at < rows_->entries.size(); ++k)
        at = subtreeEnd(at);
    return at;
}

float TreeView::turnOf(TreeNode node, bool expanded) const {
    if (const auto it = turning_.find(node.id); it != turning_.end() && it->second->running())
        return it->second->value();
    return expanded ? 1.0f : 0.0f;
}

void TreeView::turn(TreeNode node, bool expanded) {
    // from where it shows (turning back on the way), over the theme's normal motion, as a collapsible panel's arrow
    const float from = turnOf(node, !expanded);
    std::erase_if(turning_, [](const auto& entry) { return !entry.second->running(); });
    auto& animation = turning_[node.id];
    if (!animation)
        animation = std::make_unique<Animation>([this] { invalidatePaint(); });
    animation->set(from);
    const UiContext* ui = context();
    animation->animateTo(scheduler(), expanded ? 1.0f : 0.0f, ui ? Duration(ui->theme.motion.normal) : Duration::zero(), Easing::OutCubic);
    if (!animation->running())
        turning_.erase(node.id);
}

void TreeView::expandAt(std::size_t index) {
    Entry& entry = rows_->entries[index];
    if (!entry.expandable || entry.expanded)
        return;
    turn(entry.node, true);
    entry.expanded = true;
    expanded_.insert(entry.node.id);
    std::vector<Entry> children;
    appendChildren(entry.node, entry.depth + 1, children);
    rows_->entries.insert(rows_->entries.begin() + static_cast<std::ptrdiff_t>(index + 1), children.begin(), children.end());
    rows_->notifyChanged(index, 1);
    if (!children.empty())
        rows_->notifyInserted(index + 1, children.size());
}

void TreeView::collapseAt(std::size_t index) {
    Entry& entry = rows_->entries[index];
    if (!entry.expanded)
        return;
    turn(entry.node, false);
    entry.expanded = false;
    expanded_.erase(entry.node.id);
    opened_.erase(entry.node.id);
    const std::size_t end = subtreeEnd(index);
    // the current one under it: the node collapsed is current then
    const std::optional<std::size_t> current = list_->current();
    const bool under = current && *current > index && *current < end;
    if (end > index + 1) {
        rows_->entries.erase(rows_->entries.begin() + static_cast<std::ptrdiff_t>(index + 1), rows_->entries.begin() + static_cast<std::ptrdiff_t>(end));
        rows_->notifyRemoved(index + 1, end - index - 1);
    }
    rows_->notifyChanged(index, 1);
    if (under)
        list_->setCurrent(index);
}

void TreeView::toggle(std::size_t index) {
    if (index >= rows_->entries.size())
        return;
    if (rows_->entries[index].expanded)
        collapseAt(index);
    else
        expandAt(index);
}

void TreeView::setExpanded(TreeNode node, bool expanded) {
    if (const std::optional<std::size_t> index = shownIndexOf(node)) {
        if (expanded)
            expandAt(*index);
        else
            collapseAt(*index);
    } else if (expanded) {
        expanded_.insert(node.id);
    } else {
        expanded_.erase(node.id);
    }
}

void TreeView::expandAll() {
    if (!model_)
        return;
    // every node with children, all the way down
    std::vector<std::optional<TreeNode>> parents{std::nullopt};
    while (!parents.empty()) {
        const std::optional<TreeNode> parent = parents.back();
        parents.pop_back();
        const std::size_t count = model_->childCount(parent);
        for (std::size_t i = 0; i < count; ++i) {
            const TreeNode node = model_->child(parent, i);
            if (model_->hasChildren(node)) {
                expanded_.insert(node.id);
                parents.push_back(node);
            }
        }
    }
    rebuild();
}

void TreeView::collapseAll() {
    expanded_.clear();
    rebuild();
}

// ---- keys ----

void TreeView::onPreviewKey(KeyEvent& event) {
    if (!event.pressed || !list_->hasFocus())
        return;
    const std::optional<std::size_t> current = list_->current();
    if (!current)
        return;
    // toward the end side: in; toward the start side: out
    const bool rtl = rightToLeft();
    const bool in = event.key == (rtl ? platform::Key::Left : platform::Key::Right);
    const bool out = event.key == (rtl ? platform::Key::Right : platform::Key::Left);
    if (!in && !out)
        return;
    event.handled = true;
    const Entry& entry = rows_->entries[*current];
    if (in) {
        if (entry.expandable && !entry.expanded)
            expandAt(*current);
        else if (entry.expanded && *current + 1 < rows_->entries.size())
            list_->setCurrent(*current + 1);
    } else if (entry.expanded) {
        collapseAt(*current);
    } else {
        // to its parent: the entry before it one level up
        for (std::size_t i = *current; i-- > 0;) {
            if (rows_->entries[i].depth < entry.depth) {
                list_->setCurrent(i);
                break;
            }
        }
    }
}

// ---- the model's changes ----

void TreeView::changesBegun() {
    ++batch_;
}

void TreeView::changesEnded() {
    if (batch_ > 0 && --batch_ == 0 && std::exchange(reorderLater_, false))
        reorder();
}

void TreeView::childrenInserted(std::optional<TreeNode> parent, std::size_t first, std::size_t count) {
    if (ordered()) {        // where they go is the order's (once, at the end of a batch)
        if (batch_ > 0)
            reorderLater_ = true;
        else
            reorder();
        return;
    }
    if (parent) {
        // a parent that shows collapsed: it may have children now
        const std::optional<std::size_t> index = shownIndexOf(*parent);
        if (!index)
            return;
        Entry& entry = rows_->entries[*index];
        entry.expandable = model_->hasChildren(*parent);
        if (!entry.expanded) {
            rows_->notifyChanged(*index, 1);
            return;
        }
    }
    const std::optional<std::size_t> at = childPosition(parent, first);
    if (!at)
        return;
    const std::uint32_t depth = parent ? rows_->entries[*shownIndexOf(*parent)].depth + 1 : 0;
    std::vector<Entry> added;
    for (std::size_t i = first; i < first + count; ++i) {
        const TreeNode node = model_->child(parent, i);
        const bool expandable = model_->hasChildren(node);
        const bool expanded = expandable && expanded_.contains(node.id);
        added.push_back({node, depth, expandable, expanded});
        if (expanded)
            appendChildren(node, depth + 1, added);
    }
    rows_->entries.insert(rows_->entries.begin() + static_cast<std::ptrdiff_t>(*at), added.begin(), added.end());
    rows_->notifyInserted(*at, added.size());
}

void TreeView::childrenRemoved(std::optional<TreeNode> parent, std::size_t first, std::size_t count) {
    if (ordered()) {
        if (batch_ > 0)
            reorderLater_ = true;
        else
            reorder();
        return;
    }
    const std::optional<std::size_t> at = childPosition(parent, first);
    if (at) {
        std::size_t end = *at;
        for (std::size_t k = 0; k < count && end < rows_->entries.size(); ++k)
            end = subtreeEnd(end);
        for (std::size_t i = *at; i < end; ++i)
            expanded_.erase(rows_->entries[i].node.id);
        if (end > *at) {
            rows_->entries.erase(rows_->entries.begin() + static_cast<std::ptrdiff_t>(*at), rows_->entries.begin() + static_cast<std::ptrdiff_t>(end));
            rows_->notifyRemoved(*at, end - *at);
        }
    }
    // the parent may have no children now
    if (parent) {
        if (const std::optional<std::size_t> index = shownIndexOf(*parent)) {
            rows_->entries[*index].expandable = model_->hasChildren(*parent);
            rows_->notifyChanged(*index, 1);
        }
    }
}

void TreeView::childrenChanged(std::optional<TreeNode> parent, std::size_t first, std::size_t count) {
    if (ordered()) {        // they may go elsewhere, or not show
        if (batch_ > 0)
            reorderLater_ = true;
        else
            reorder();
        return;
    }
    std::optional<std::size_t> at = childPosition(parent, first);
    for (std::size_t k = 0; at && k < count && *at < rows_->entries.size(); ++k) {
        Entry& entry = rows_->entries[*at];
        entry.node = model_->child(parent, first + k);
        entry.expandable = model_->hasChildren(entry.node);
        rows_->notifyChanged(*at, 1);
        at = subtreeEnd(*at);
    }
}

void TreeView::modelReset() {
    expanded_.clear();
    reorder();
}

} // namespace glw
