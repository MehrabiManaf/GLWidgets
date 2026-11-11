// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// Multi-channel signed distance fields of outlines (Chlumsky's MSDF): an image whose three channels are each the distance
// to the nearest edge of a "color", so that the median of the three, interpolated, keeps a shape's corners sharp at any
// scale (one channel's field would round them). For glyphs that zoom (GlyphAtlas::fieldGlyph).
//
// An outline is given as contours of points: its curves flattened into short lines, finely enough not to show (a
// fraction of a field's pixel off), the points where two of its own segments meet marked as joints. Its edges are split
// at the joints where it turns (more than eight degrees) and colored so that the edges on the two sides of a corner
// share one channel only; a contour without corners is in all three. For each pixel each channel takes the edge of its
// color nearest to it and the distance to it, extended past the edge's ends along it (a pseudo-distance: what makes a
// corner a corner in the median). Where the median then says inside and the outline says outside, or the other way (two
// contours close together), the pixel takes the true distance in all three channels instead.
#include <vector>

namespace glw::text::detail {

struct FieldPoint {
    double x = 0;
    double y = 0;
    bool joint = true;          // where two of the outline's segments meet (not a point within a flattened curve)
};

// Closed: the last point goes back to the first. Its points in the field's pixels, y up.
using FieldContour = std::vector<FieldPoint>;

// The field of `contours` over `width` x `height` pixels (pixel (x, y)'s center at x + 0.5, y + 0.5; its rows from the
// bottom up): three channels a pixel, 0.5 on the outline, 1 `range` / 2 pixels inside it, 0 as far outside. Inside is
// by the non-zero rule, whichever way the contours turn.
std::vector<float> multiChannelField(const std::vector<FieldContour>& contours, int width, int height, double range);

} // namespace glw::text::detail
