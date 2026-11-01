// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// A borderless window with chrome of its own, drawn by the painter: a title bar holding major tabs (as a browser's),
// window buttons, resize borders given to the system by hit testing. Each tab shows a part of the painter: a chart, the
// shapes, a node canvas that zooms with the mouse wheel. There are no widgets yet: this is what they will be made of.
//
//   paint_demo [--tab N] [--frames N]
//
// --tab N opens on tab N (0 to 2). --frames N draws N frames in a row (switching tabs) and exits: a smoke test of the
// painter and the renderer.
#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Paint/Renderer.hpp>
#include <GLWidgets/Platform/Platform.hpp>
#include <GLWidgets/Rhi/Device.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace {

namespace paint = glw::paint;
namespace platform = glw::platform;
using glw::Color;
using glw::Point;
using glw::Rect;
using glw::Size;

template <class... F>
struct Overloaded : F... {
    using F::operator()...;
};
template <class... F>
Overloaded(F...) -> Overloaded<F...>;

constexpr float captionHeight = 40;
constexpr float buttonWidth = 46;
constexpr float resizeBorder = 6;
constexpr float tabWidth = 180;
constexpr int tabCount = 3;

namespace colors {
constexpr Color caption{0.09f, 0.10f, 0.12f, 1};
constexpr Color content{0.15f, 0.16f, 0.19f, 1};
constexpr Color tabIdle{0.12f, 0.13f, 0.16f, 1};
constexpr Color tabHover{0.13f, 0.14f, 0.17f, 1};
constexpr Color card{0.19f, 0.20f, 0.24f, 1};
constexpr Color line{0.30f, 0.32f, 0.38f, 1};
constexpr Color text{0.70f, 0.72f, 0.78f, 1};
constexpr Color glyph{0.85f, 0.86f, 0.90f, 1};
constexpr Color hover{1, 1, 1, 0.08f};
constexpr Color closeHover{0.80f, 0.16f, 0.18f, 1};
constexpr Color shadow{0, 0, 0, 0.45f};
constexpr Color series[] = {{0.35f, 0.65f, 1.00f, 1}, {1.00f, 0.60f, 0.25f, 1}, {0.45f, 0.85f, 0.50f, 1}};
} // namespace colors

enum class Part { None, Tab0, Tab1, Tab2, Minimize, Maximize, Close };

struct View {
    std::unique_ptr<platform::Window> window;
    std::unique_ptr<glw::rhi::Surface> surface;     // after the window: destroyed before it
    paint::DrawList list;
    paint::RenderBuffers buffers;
    int activeTab = 0;
    Part hovered = Part::None;
    Part pressed = Part::None;
    float zoom = 1;                 // of the node canvas
    bool dirty = true;
};

Rect tabRect(int i) {
    return {12 + i * (tabWidth + 4), 8, tabWidth, captionHeight - 8};
}

Rect buttonRect(Part button, Size window) {
    const int index = button == Part::Close ? 0 : button == Part::Maximize ? 1 : 2;     // from the right
    return {window.width - (index + 1) * buttonWidth, 0, buttonWidth, captionHeight};
}

// The part of the chrome at `p`: a tab, a window button, or none (the caption, or the content).
Part partAt(Point p, Size window) {
    if (p.y >= captionHeight)
        return Part::None;
    for (Part button : {Part::Close, Part::Maximize, Part::Minimize}) {
        if (buttonRect(button, window).contains(p))
            return button;
    }
    for (int i = 0; i < tabCount; ++i) {
        if (tabRect(i).contains(p))
            return static_cast<Part>(static_cast<int>(Part::Tab0) + i);
    }
    return Part::None;
}

platform::HitArea hitTest(const platform::Window& window, Point p) {
    using platform::HitArea;
    const Size s = window.size();
    if (window.state() == platform::WindowState::Normal) {
        const bool left = p.x < resizeBorder, right = p.x >= s.width - resizeBorder;
        const bool top = p.y < resizeBorder, bottom = p.y >= s.height - resizeBorder;
        if (top && left) return HitArea::ResizeTopLeft;
        if (top && right) return HitArea::ResizeTopRight;
        if (bottom && left) return HitArea::ResizeBottomLeft;
        if (bottom && right) return HitArea::ResizeBottomRight;
        if (top) return HitArea::ResizeTop;
        if (bottom) return HitArea::ResizeBottom;
        if (left) return HitArea::ResizeLeft;
        if (right) return HitArea::ResizeRight;
    }
    if (p.y < captionHeight && partAt(p, s) == Part::None)
        return HitArea::Caption;
    return HitArea::Client;
}

// ---- the chrome ----

void paintTitleBar(paint::Painter& p, const View& view, Size size) {
    p.fillRect({0, 0, size.width, captionHeight}, colors::caption);

    for (int i = 0; i < tabCount; ++i) {
        const Rect tab = tabRect(i);
        const bool active = i == view.activeTab;
        const Part part = static_cast<Part>(static_cast<int>(Part::Tab0) + i);
        const Color background = active ? colors::content : view.hovered == part ? colors::tabHover : colors::tabIdle;
        // rounded at the top only: the bottom corners go below the strip and are cut
        p.pushClip({tab.x, tab.y, tab.width, captionHeight - tab.y});
        p.fillRoundedRect({tab.x, tab.y, tab.width, tab.height + 12}, 8, background);
        p.popClip();
        // where its icon and title will be, once there is text
        const float middle = tab.y + tab.height / 2;
        p.fillCircle({tab.x + 18, middle}, 5, colors::series[i]);
        p.fillRoundedRect({tab.x + 32, middle - 3, 96.0f - 18 * i, 6}, 3, active ? colors::text : colors::line);
    }

    for (Part button : {Part::Minimize, Part::Maximize, Part::Close}) {
        const Rect r = buttonRect(button, size);
        if (view.hovered == button)
            p.fillRect(r, button == Part::Close ? colors::closeHover : colors::hover);
        const Point c = r.center();
        switch (button) {
        case Part::Minimize:
            p.drawLine({c.x - 5, c.y}, {c.x + 5, c.y}, 1, colors::glyph);
            break;
        case Part::Maximize:
            if (view.window->state() == platform::WindowState::Maximized) {
                p.strokeRoundedRect({c.x - 3, c.y - 5, 8, 8}, 1, 1, colors::glyph);
                p.fillRect({c.x - 5, c.y - 3, 8, 8}, view.hovered == button ? colors::tabHover : colors::caption);
                p.strokeRoundedRect({c.x - 5, c.y - 3, 8, 8}, 1, 1, colors::glyph);
            } else {
                p.strokeRoundedRect({c.x - 5, c.y - 5, 10, 10}, 1, 1, colors::glyph);
            }
            break;
        default:
            p.drawLine({c.x - 5, c.y - 5}, {c.x + 5, c.y + 5}, 1, colors::glyph);
            p.drawLine({c.x - 5, c.y + 5}, {c.x + 5, c.y - 5}, 1, colors::glyph);
            break;
        }
    }
}

// ---- the tabs ----

Rect card(paint::Painter& p, const Rect& r) {
    p.drawShadow({r.x, r.y + 4, r.width, r.height}, 10, 14, colors::shadow);
    p.fillRoundedRect(r, 10, colors::card);
    p.strokeRoundedRect(r, 10, 1, colors::line);
    return r;
}

void paintChart(paint::Painter& p, const Rect& area) {
    const Rect c = card(p, area.inflated(-24));
    const Rect plot{c.x + 48, c.y + 32, c.width - 80, c.height - 72};
    if (plot.empty())
        return;
    for (int i = 0; i <= 10; ++i) {
        const float x = plot.x + plot.width * i / 10, y = plot.y + plot.height * i / 10;
        p.drawLine({x, plot.top()}, {x, plot.bottom()}, 1, colors::line);
        p.drawLine({plot.left(), y}, {plot.right(), y}, 1, colors::line);
    }

    p.pushClip(plot);
    constexpr int samples = 1200;
    std::vector<Point> points(samples);
    for (int s = 0; s < 3; ++s) {
        for (int i = 0; i < samples; ++i) {
            const float t = static_cast<float>(i) / (samples - 1);
            const float v = std::sin(t * (6 + 4 * s) + s) * std::exp(-t * (0.6f + s * 0.4f)) +
                            0.08f * std::sin(t * 90.0f + s * 2.0f);
            points[i] = {plot.x + plot.width * t, plot.y + plot.height * (0.5f - 0.4f * v)};
        }
        p.drawPolyline(points, 2, colors::series[s]);
        for (int i = 0; i < samples; i += 100) {
            p.fillCircle(points[i], 4.5f, colors::series[s]);
            p.strokeCircle(points[i], 4.5f, 1.5f, colors::card);
        }
    }
    p.popClip();
}

void paintShapes(paint::Painter& p, const Rect& area) {
    float x = area.x + 32, y = area.y + 32;
    // corner radii, borders
    for (int i = 0; i < 6; ++i) {
        const Rect r{x + i * 110.0f, y, 90, 64};
        card(p, r);
        p.strokeRoundedRect(r.inflated(-10), static_cast<float>(i * 5), 1.0f + i, colors::series[i % 3]);
    }
    y += 100;
    // shadows of growing blur, circles
    for (int i = 0; i < 6; ++i) {
        const Rect r{x + i * 110.0f, y, 90, 64};
        p.drawShadow({r.x, r.y + 6, r.width, r.height}, 12, 2.0f + i * 6, colors::shadow);
        p.fillRoundedRect(r, 12, colors::card);
        p.fillCircle(r.center(), 6.0f + i * 3, colors::series[i % 3]);
    }
    y += 110;
    // lines of every width, at every angle
    const float widths[] = {0.5f, 1, 1.5f, 2, 4, 8};
    for (int i = 0; i < 6; ++i) {
        const Point c{x + 45 + i * 110.0f, y + 45};
        for (int a = 0; a < 12; ++a) {
            const float angle = a * 3.14159265f / 12;
            p.drawLine(c, {c.x + 40 * std::cos(angle), c.y - 40 * std::sin(angle)}, widths[i], colors::glyph);
        }
    }
    y += 80;
    // convex polygons
    for (int i = 0; i < 6; ++i) {
        const Point c{x + 45 + i * 110.0f, y + 45};
        std::vector<Point> polygon;
        for (int k = 0; k < 3 + i; ++k) {
            const float angle = k * 2 * 3.14159265f / (3 + i);
            polygon.push_back({c.x + 36 * std::cos(angle), c.y + 36 * std::sin(angle)});
        }
        p.fillConvexPolygon(polygon, colors::series[i % 3]);
    }
}

// A cubic Bézier curve as points, for the connections of nodes.
std::vector<Point> bezier(Point a, Point b, Point c, Point d, int segments) {
    std::vector<Point> points;
    for (int i = 0; i <= segments; ++i) {
        const float t = static_cast<float>(i) / segments, u = 1 - t;
        points.push_back(a * (u * u * u) + b * (3 * u * u * t) + c * (3 * u * t * t) + d * (t * t * t));
    }
    return points;
}

void paintNodes(paint::Painter& p, const Rect& area, float zoom) {
    p.pushClip(area);
    // the canvas: its origin at the top left of the area, zoomed around it
    p.pushTransform({zoom, {area.x + 40, area.y + 40}});
    for (int i = 0; i < 30; ++i) {
        p.drawLine({i * 40.0f, 0}, {i * 40.0f, 1200}, 1, {1, 1, 1, 0.04f});
        p.drawLine({0, i * 40.0f}, {1200, i * 40.0f}, 1, {1, 1, 1, 0.04f});
    }
    const Rect nodes[] = {{0, 40, 200, 120}, {320, 0, 200, 160}, {320, 220, 200, 100}, {640, 100, 200, 140}};
    const int links[][2] = {{0, 1}, {0, 2}, {1, 3}, {2, 3}};
    for (const auto& link : links) {
        const Rect& from = nodes[link[0]];
        const Rect& to = nodes[link[1]];
        const Point a{from.right(), from.y + 56}, d{to.x, to.y + 56};
        p.drawPolyline(bezier(a, {a.x + 80, a.y}, {d.x - 80, d.y}, d, 48), 3, colors::series[link[0] % 3]);
    }
    for (int i = 0; i < 4; ++i) {
        const Rect& n = nodes[i];
        card(p, n);
        p.pushClip({n.x, n.y, n.width, 32});
        p.fillRoundedRect(n, 10, colors::series[i % 3]);
        p.popClip();
        p.fillRoundedRect({n.x + 12, n.y + 13, 100.0f - i * 10, 6}, 3, colors::caption);
        p.fillCircle({n.x, n.y + 56}, 6, colors::glyph);
        p.fillCircle({n.right(), n.y + 56}, 6, colors::glyph);
    }
    p.popTransform();
    p.popClip();
}

void paintView(View& view) {
    view.list.clear();
    const Size size = view.window->size();
    paint::Painter p(view.list, {0, 0, size.width, size.height}, view.window->scale());
    paintTitleBar(p, view, size);
    const Rect content{0, captionHeight, size.width, size.height - captionHeight};
    p.fillRect(content, colors::content);
    switch (view.activeTab) {
    case 0: paintChart(p, content); break;
    case 1: paintShapes(p, content); break;
    default: paintNodes(p, content, view.zoom); break;
    }
}

} // namespace

int main(int argc, char** argv) {
    int frames = -1;    // -1: draw when needed, until the window is closed
    int firstTab = 0;
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--frames" && i + 1 < argc)
            frames = std::max(1, std::atoi(argv[++i]));
        else if (std::string_view(argv[i]) == "--tab" && i + 1 < argc)
            firstTab = std::clamp(std::atoi(argv[++i]), 0, tabCount - 1);
    }

    try {
        auto system = platform::createPlatform();
#ifdef NDEBUG
        auto device = glw::rhi::createDevice(*system);
#else
        auto device = glw::rhi::createDevice(*system, {.debug = true});
#endif
        paint::Renderer renderer(*device);
        std::printf("platform: %s, GPU: %s\n", std::string(system->name()).c_str(), std::string(device->backendName()).c_str());

        std::vector<std::unique_ptr<View>> views;
        {
            auto view = std::make_unique<View>();
            view->window = system->createWindow({.title = "GLWidgets — Paint", .size = {1100, 720}, .borderless = true});
            view->surface = device->createSurface(*view->window);
            view->window->setHitTest([window = view->window.get()](Point p) { return hitTest(*window, p); });
            view->activeTab = firstTab;
            view->window->show();
            const Size size = view->window->size();
            const glw::PixelSize pixels = view->window->pixelSize();
            std::printf("window: %gx%g dips, %dx%d pixels, scale %g\n", size.width, size.height, pixels.width, pixels.height,
                        view->window->scale());
            views.push_back(std::move(view));
        }

        auto find = [&](platform::WindowId id) -> View* {
            for (auto& v : views) {
                if (v->window->id() == id)
                    return v.get();
            }
            return nullptr;
        };
        auto close = [&](platform::WindowId id) {
            std::erase_if(views, [&](const auto& v) { return v->window->id() == id; });
        };

        int drawn = 0;
        bool quit = false;
        while (!quit && !views.empty()) {
            const bool dirty = std::any_of(views.begin(), views.end(), [](const auto& v) { return v->dirty; });
            if (frames < 0 && !dirty)
                system->waitForEvents();

            platform::Event event;
            while (!quit && system->pollEvent(event)) {
                std::visit(Overloaded{
                               [&](const platform::QuitRequested&) { quit = true; },
                               [&](const platform::WindowCloseRequested& e) { close(e.window); },
                               [&](const platform::WindowResized& e) {
                                   if (View* v = find(e.window))
                                       v->dirty = true;
                               },
                               [&](const platform::WindowScaleChanged& e) {
                                   if (View* v = find(e.window))
                                       v->dirty = true;
                               },
                               [&](const platform::WindowExposed& e) {
                                   if (View* v = find(e.window))
                                       v->dirty = true;
                               },
                               [&](const platform::WindowStateChanged& e) {
                                   if (View* v = find(e.window))
                                       v->dirty = true;
                               },
                               [&](const platform::MouseMoved& e) {
                                   View* v = find(e.window);
                                   if (!v)
                                       return;
                                   const Part part = partAt(e.position, v->window->size());
                                   if (part != v->hovered) {
                                       v->hovered = part;
                                       v->dirty = true;
                                   }
                               },
                               [&](const platform::MouseCrossed& e) {
                                   View* v = find(e.window);
                                   if (v && !e.entered && v->hovered != Part::None) {
                                       v->hovered = Part::None;
                                       v->dirty = true;
                                   }
                               },
                               [&](const platform::MouseButtonChanged& e) {
                                   View* v = find(e.window);
                                   if (!v || e.button != platform::MouseButton::Left)
                                       return;
                                   const Part part = partAt(e.position, v->window->size());
                                   if (e.pressed) {
                                       v->pressed = part;
                                       if (part >= Part::Tab0 && part <= Part::Tab2) {
                                           v->activeTab = static_cast<int>(part) - static_cast<int>(Part::Tab0);
                                           v->dirty = true;
                                       }
                                       return;
                                   }
                                   // a window button acts when released over the button it was pressed on
                                   const Part pressed = std::exchange(v->pressed, Part::None);
                                   if (part != pressed)
                                       return;
                                   if (part == Part::Close)
                                       close(e.window);
                                   else if (part == Part::Minimize)
                                       v->window->minimize();
                                   else if (part == Part::Maximize) {
                                       if (v->window->state() == platform::WindowState::Maximized)
                                           v->window->restore();
                                       else
                                           v->window->maximize();
                                   }
                               },
                               [&](const platform::MouseWheelScrolled& e) {
                                   View* v = find(e.window);
                                   if (v && v->activeTab == 2) {
                                       v->zoom = std::clamp(v->zoom * std::pow(1.15f, e.delta.y), 0.25f, 4.0f);
                                       v->dirty = true;
                                   }
                               },
                               [&](const platform::KeyChanged& e) {
                                   if (e.pressed && e.key == platform::Key::Escape)
                                       quit = true;
                               },
                               [](const auto&) {},
                           },
                           event);
            }
            if (quit)
                break;

            if (frames >= 0) {
                for (auto& v : views) {
                    v->activeTab = (drawn / 10) % tabCount;
                    v->dirty = true;
                }
            }
            if (std::none_of(views.begin(), views.end(), [](const auto& v) { return v->dirty; }))
                continue;

            auto commands = device->beginCommands();
            for (auto& v : views) {
                if (!v->dirty)
                    continue;
                paintView(*v);
                renderer.upload(*commands, v->list, v->buffers);
                if (auto size = commands->beginSurfacePass(*v->surface, colors::content)) {
                    renderer.draw(*commands, v->list, v->buffers, v->surface->format(), *size, v->window->scale());
                    commands->endPass();
                }
                v->dirty = false;
            }
            commands->submit();

            if (frames >= 0 && ++drawn == frames) {
                std::printf("drew %d frames, %zu vertices in the last\n", drawn, views.front()->list.vertices.size());
                break;
            }
        }
        views.clear();      // the surfaces before the device, the windows before the platform
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "paint_demo: %s\n", e.what());
        return 1;
    }
}
