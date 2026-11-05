// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// A hierarchy of sizes as tiles: each node a rectangle as large as its share of its parent, its children in it (a disk's
// folders and files, a market's sectors and their companies). The tiles are laid out squarified (Bruls, Huizing and van
// Wijk): as near to squares as they can be, so that they are easy to compare and to label, filling all of the room. A few
// levels show at once (setDepth): a node with children shows its name along its top and its children under it.
//
// Zooming is going into a node: a click on a tile with children (or the wheel turned in over it) makes it fill the
// chart, its tiles growing out of where they were; the path to it shows along the top, each of its names a way back
// there, and a right click, Backspace or the wheel turned out goes back up a level. New data (setRoot) is eased to as
// well: each tile from where it was (tiles matched by the names on their paths), so that a live hierarchy moves rather
// than jumps.
//
// The top level's tiles take the style's series colors in turn (or their own), and their children shades of their
// parent's; the tile under the mouse is lit, with its path, its value and its share of its parent by the mouse.
#include <GLWidgets/Charts/Chart.hpp>
#include <GLWidgets/Export.hpp>

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace glw {

struct TreeMapNode {
    LocalizedText label;
    double value = 0;                   // a leaf's; a node with children is as large as all of them
    std::optional<Color> color;         // none: its parent's shade, or (at the top) the style's, in turn
    std::vector<TreeMapNode> children;

    // Its value, or its children's together.
    double total() const;
};

class GLW_EXPORT TreeMap : public Chart {
public:
    struct Props {
        LocalizedText title;
        int depth = 2;                  // the levels shown at once under the node zoomed into
    };

    explicit TreeMap(Props props = {});
    ~TreeMap() override;

    // Its data (eased to, from what shows). What it was zoomed into stays, as far down as the new data has it.
    void setRoot(TreeMapNode root);
    const TreeMapNode& root() const { return root_; }
    void setDepth(int depth);

    // The node that fills it, as the places of the nodes on the way to it from the root (empty: the root).
    void setFocus(std::vector<std::size_t> path);
    const std::vector<std::size_t>& focus() const { return focus_; }
    // A level up; false at the root.
    bool zoomOut();

    // How a value shows (in a tile, in the tooltip); none: formatNumber.
    void setFormatter(std::function<std::string(double)> format);
    // A leaf clicked (a node with children is zoomed into instead): its path.
    void setOnLeafClicked(std::function<void(std::span<const std::size_t> path)> handler) { onLeafClicked_ = std::move(handler); }

    // The node whose tile is at a point (its own coordinates) as it was last painted: its path; the deepest shown there.
    std::optional<std::vector<std::size_t>> nodeAt(Point at) const;
    // Where a node's tile is (its path), as it shows now: none when it does not show.
    std::optional<Rect> tileRect(std::span<const std::size_t> path) const;

protected:
    void paint(PaintContext& context) override;
    void onMouse(MouseEvent& event) override;
    void onKey(KeyEvent& event) override;
    void onHoverChanged(bool hovered) override;

private:
    struct Tile {
        std::string key;                // the names on its path, to match it with the tile it was
        std::vector<std::size_t> path;
        Rect rect;                      // where it goes
        Color color;
        int level = 0;                  // 0: a child of the node zoomed into
        bool parent = false;            // its children shown in it
        const TreeMapNode* node = nullptr;
    };
    const TreeMapNode* nodeOf(std::span<const std::size_t> path) const;
    // Its tiles laid out in `area`, for the node of `path` and the levels under it.
    void layOut(const TreeMapNode& node, std::vector<std::size_t>& path, const std::string& key, Rect area, int level,
                Color color, std::vector<Tile>& out) const;
    void changed(bool eased);
    Rect shownRect(const Tile& tile) const;
    std::string format(double value) const;
    float headerHeight() const;

    Props props_;
    TreeMapNode root_;
    std::vector<std::size_t> focus_;
    std::vector<Tile> tiles_;                           // as last laid out
    Rect laidOutIn_;
    bool stale_ = true;
    bool eased_ = false;                                // the next layout eased to from what shows
    std::unordered_map<std::string, Rect> from_;        // where the tiles were when it changed
    Animation progress_;
    std::optional<Point> mouse_;
    std::optional<std::size_t> hovered_;                // in tiles_
    struct Crumb {
        Rect rect;
        std::size_t depth;                              // how much of the focus's path
    };
    std::vector<Crumb> crumbs_;
    std::function<std::string(double)> format_;
    std::function<void(std::span<const std::size_t>)> onLeafClicked_;
};

} // namespace glw
