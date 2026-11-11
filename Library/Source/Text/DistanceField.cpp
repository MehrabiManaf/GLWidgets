// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include "Text/DistanceField.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace glw::text::detail {

namespace {

// a channel's bit; an edge's color is the channels it is in
constexpr int red = 1, green = 2, blue = 4;
constexpr int yellow = red | green, magenta = red | blue, cyan = green | blue, white = red | green | blue;

struct Vec {
    double x = 0, y = 0;
};
Vec operator-(Vec a, Vec b) {
    return {a.x - b.x, a.y - b.y};
}
double dot(Vec a, Vec b) {
    return a.x * b.x + a.y * b.y;
}
double cross(Vec a, Vec b) {
    return a.x * b.y - a.y * b.x;
}
double length(Vec a) {
    return std::sqrt(dot(a, a));
}
Vec normalized(Vec a) {
    const double l = length(a);
    return l > 0 ? Vec{a.x / l, a.y / l} : Vec{0, 1};
}

struct Edge {
    Vec a, b;
    int color = white;
};

// A distance to an edge: signed (its sign: which side of it), how askew the edge is to the line to the point (0: square
// to it; what tells two edges as near apart: the one more square to it is the nearer), where along it (0 to 1: within).
struct Distance {
    double distance = -std::numeric_limits<double>::max();
    double askew = 1;
    double along = 0;
    const Edge* edge = nullptr;
};

bool closer(const Distance& a, const Distance& b) {
    const double da = std::abs(a.distance), db = std::abs(b.distance);
    return da < db || (da == db && a.askew < b.askew);
}

Distance distanceTo(const Edge& e, Vec p) {
    const Vec aq = p - e.a, ab = e.b - e.a;
    const double l2 = dot(ab, ab);
    if (l2 <= 0)
        return {length(aq), 1, 0, &e};
    const double along = dot(aq, ab) / l2;
    const Vec eq = (along > 0.5 ? e.b : e.a) - p;
    const double toEnd = length(eq);
    if (along > 0 && along < 1) {
        const double square = cross(aq, ab) / std::sqrt(l2);
        if (std::abs(square) < toEnd)
            return {square, 0, along, &e};
    }
    const double side = cross(aq, ab) >= 0 ? 1.0 : -1.0;
    return {side * toEnd, std::abs(dot(normalized(ab), normalized(eq))), along, &e};
}

// Past an edge's end: the distance to its line there instead, when that is nearer (a corner stays one).
void extend(Distance& d, Vec p) {
    if (!d.edge)
        return;
    const Edge& e = *d.edge;
    const Vec direction = normalized(e.b - e.a);
    if (d.along < 0) {
        const Vec aq = p - e.a;
        if (dot(aq, direction) < 0) {
            const double pseudo = cross(aq, direction);
            if (std::abs(pseudo) <= std::abs(d.distance)) {
                d.distance = pseudo;
                d.askew = 0;
            }
        }
    } else if (d.along > 1) {
        const Vec bq = p - e.b;
        if (dot(bq, direction) > 0) {
            const double pseudo = cross(bq, direction);
            if (std::abs(pseudo) <= std::abs(d.distance)) {
                d.distance = pseudo;
                d.askew = 0;
            }
        }
    }
}

// The next color after `color`, not sharing two channels with `banned` (the first color of a contour, for its last
// stretch: so that they too meet as two at a corner should).
int nextColor(int color, int banned) {
    if (color == 0 || color == white)
        return cyan;
    const int both = color & banned;
    if (both == red || both == green || both == blue)
        return both ^ white;
    const int shifted = color << 1;
    return (shifted | shifted >> 3) & white;
}

// A contour's edges, colored: split at its corners (where it turns more than about eight degrees, at a joint), each
// stretch between two of them a color of its own; one corner: three stretches (a teardrop); none: all channels.
void colorContour(std::vector<Edge>& edges, const std::vector<bool>& joints) {
    const std::size_t n = edges.size();
    if (n == 0)
        return;
    const double sharp = std::sin(3.0);         // (the cross product of two directions past it: a corner)
    std::vector<std::size_t> corners;
    for (std::size_t i = 0; i < n; ++i) {
        if (!joints[i])
            continue;
        const Vec before = normalized(edges[(i + n - 1) % n].b - edges[(i + n - 1) % n].a);
        const Vec after = normalized(edges[i].b - edges[i].a);
        if (dot(before, after) <= 0 || std::abs(cross(before, after)) > sharp)
            corners.push_back(i);
    }
    if (corners.empty()) {
        for (Edge& e : edges)
            e.color = white;
        return;
    }
    if (corners.size() == 1) {
        // a teardrop: from its corner, a third of it one color, a third all, a third another
        const int colors[3] = {magenta, white, yellow};
        for (std::size_t k = 0; k < n; ++k) {
            const int third = n > 1 ? static_cast<int>(3 * k / n) : 1;
            edges[(corners[0] + k) % n].color = colors[std::min(third, 2)];
        }
        return;
    }
    const std::size_t count = corners.size();
    int color = nextColor(white, 0);
    const int first = color;
    std::size_t stretch = 0;
    for (std::size_t k = 0; k < n; ++k) {
        const std::size_t index = (corners[0] + k) % n;
        if (stretch + 1 < count && corners[stretch + 1] == index) {
            ++stretch;
            color = nextColor(color, stretch == count - 1 ? first : 0);
        }
        edges[index].color = color;
    }
}

double median(double a, double b, double c) {
    return std::max(std::min(a, b), std::min(std::max(a, b), c));
}

} // namespace

std::vector<float> multiChannelField(const std::vector<FieldContour>& contours, int width, int height, double range) {
    std::vector<float> out(static_cast<std::size_t>(std::max(0, width)) * std::max(0, height) * 3, 0.0f);
    // the edges, colored contour by contour; which way the outline turns (its area's sign), for which side is inside
    std::vector<Edge> edges;
    double area = 0;
    for (const FieldContour& contour : contours) {
        std::vector<Edge> own;
        std::vector<bool> joints;
        for (std::size_t i = 0; i < contour.size(); ++i) {
            const FieldPoint& a = contour[i];
            const FieldPoint& b = contour[(i + 1) % contour.size()];
            area += a.x * b.y - b.x * a.y;
            if (a.x == b.x && a.y == b.y)
                continue;
            own.push_back({{a.x, a.y}, {b.x, b.y}, white});
            joints.push_back(a.joint);
        }
        colorContour(own, joints);
        edges.insert(edges.end(), own.begin(), own.end());
    }
    if (edges.empty())
        return out;
    // an edge's distance is negative on its left; inside is on the left of a contour turning counterclockwise (y up)
    const double inward = area > 0 ? -1.0 : 1.0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const Vec p{x + 0.5, y + 0.5};
            Distance nearest, channel[3];
            int winding = 0;
            for (const Edge& e : edges) {
                const Distance d = distanceTo(e, p);
                if (closer(d, nearest))
                    nearest = d;
                for (int c = 0; c < 3; ++c)
                    if ((e.color & (1 << c)) && closer(d, channel[c]))
                        channel[c] = d;
                // inside by the non-zero rule: the edges a ray to the right crosses, up or down
                if (e.a.y <= p.y) {
                    if (e.b.y > p.y && cross(e.b - e.a, p - e.a) > 0)
                        ++winding;
                } else if (e.b.y <= p.y && cross(e.b - e.a, p - e.a) < 0) {
                    --winding;
                }
            }
            const bool inside = winding != 0;
            const double truth = (inside ? 1.0 : -1.0) * std::abs(nearest.distance);
            double value[3];
            for (int c = 0; c < 3; ++c) {
                if (!channel[c].edge) {
                    value[c] = truth;
                    continue;
                }
                extend(channel[c], p);
                value[c] = inward * channel[c].distance;
            }
            // the median on the wrong side (two contours close, a corner's field crossing another's): the true distance
            if ((median(value[0], value[1], value[2]) > 0) != inside)
                value[0] = value[1] = value[2] = truth;
            float* o = &out[(static_cast<std::size_t>(y) * width + x) * 3];
            for (int c = 0; c < 3; ++c)
                o[c] = static_cast<float>(std::clamp(value[c] / range + 0.5, 0.0, 1.0));
        }
    }
    return out;
}

} // namespace glw::text::detail
