// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// A header's arrow of its order, and how its key moves as the order changes (ItemViewStyle::flipSorted): a list's header
// (ListView) and a table's column headers (TableView, a tree's).
#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/Animation.hpp>
#include <GLWidgets/Widgets/ItemModel.hpp>

#include <chrono>
#include <optional>

namespace glw::sorting {

// How long a key's move takes: a little longer than the theme's slow motion, `slow`.
Duration moveTime(std::chrono::milliseconds slow);

// A key whose order changed, `t` of the way (0 to 1), `height` high, sorted `to` now: it pops up off its header (a little
// larger), rocks toward the way of its new order and back, less and less, catching the light; settles where it was.
paint::Lifted keyMoved(float t, float height, SortOrder to);

// The arrow of `order` at `at` (its middle): up ascending, down descending. Moving (`t` below 1): turning from `from`'s
// way round to `order`'s, a little past and back (from none: growing out of its middle, a little past its size).
void paintArrow(paint::Painter& painter, Point at, SortOrder order, Color color, float t = 1, std::optional<SortOrder> from = {});

} // namespace glw::sorting
