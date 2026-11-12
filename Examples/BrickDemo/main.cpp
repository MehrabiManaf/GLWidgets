// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// Bricks: a table's cells as solid rounded boxes (Painter::drawBrick, Scene.hlsl), lit by an environment of its own, each
// its name printed on it (text from the glyphs' distance fields: sharp as it leans, turns and falls).
// Those near the mouse rise a little and lean toward it. Drop lets them fall (gravity, a turn each, a bounce off the bottom
// of the window), passing through one another where they meet (a depth buffer); Reset brings them back on springs.
// Their material is chosen above them: plastic, glossy, matte, metal or glass (frosted: the stripes under them blurred
// through them).
//
//   brick_demo [--frames N] [--style ...] [--colors ...] [--material plastic|glossy|matte|metal|glass]
#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Text/DrawText.hpp>
#include <GLWidgets/Text/Font.hpp>
#include <GLWidgets/Text/TextLayout.hpp>
#include <GLWidgets/Widgets/Application.hpp>
#include <GLWidgets/Widgets/Declarative.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace {

namespace paint = glw::paint;
namespace ui = glw::ui;
using glw::Color;
using glw::Point;

constexpr int columns = 10, rows = 8;
constexpr float cellWidth = 78, cellHeight = 34, gap = 8, thickness = 10;

struct Material {
    float roughness;
    float metal;
    bool glass = false;
};
constexpr Material materials[] = {{0.38f, 0}, {0.08f, 0}, {0.85f, 0}, {0.28f, 1}, {0.1f, 0, true}};

// A value pulled toward its target by a spring, slowed by damping.
struct Spring {
    float value = 0, velocity = 0;
    void step(float target, float dt, float stiffness = 180, float damping = 18) {
        const float force = (target - value) * stiffness - velocity * damping;
        velocity += force * dt;
        value += velocity * dt;
    }
};

// A brick of the table: where it rests, how it is now (springs), and, falling, its own motion.
struct Cell {
    Point home;
    Spring lift, leanX, leanY;
    bool falling = false;
    paint::Float3 position, velocity;
    paint::Quaternion turn;
    paint::Float3 spin;               // radians a second about x, y, z
};

class Board : public glw::Widget {
public:
    Board() {
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < columns; ++c)
                cells_.push_back({.home = {c * (cellWidth + gap), r * (cellHeight + gap)}});
    }

    void start(glw::Scheduler& scheduler) {
        last_ = std::chrono::steady_clock::now();
        tick_.start(scheduler, std::chrono::milliseconds(8), [this] { step(); }, true);
    }

    void setMaterial(std::size_t index) {
        material_ = materials[std::min(index, std::size(materials) - 1)];
        invalidatePaint();
    }

    void drop() {
        std::mt19937 random(static_cast<unsigned>(cells_.size() + dropped_++));
        std::uniform_real_distribution<float> any(-1, 1);
        for (Cell& cell : cells_) {
            if (cell.falling)
                continue;
            cell.falling = true;
            cell.position = {origin_.x + cell.home.x + cellWidth / 2, origin_.y + cell.home.y + cellHeight / 2, thickness / 2 + cell.lift.value};
            cell.velocity = {any(random) * 120, -200 + any(random) * 160, 250 + any(random) * 150};
            cell.turn = paint::Quaternion::tilt(cell.leanX.value, cell.leanY.value);
            cell.spin = {any(random) * 6, any(random) * 6, any(random) * 3};
        }
    }

    void reset() {
        for (Cell& cell : cells_) {
            if (!cell.falling)
                continue;
            // back to its place: its springs take over from where it is
            cell.falling = false;
            cell.lift.value = std::clamp(cell.position.z - thickness / 2, -200.0f, 400.0f);
            cell.lift.velocity = 0;
            cell.leanX.value = cell.leanY.value = 0;
            fly_[static_cast<std::size_t>(&cell - cells_.data())] = {cell.position.x - (origin_.x + cell.home.x + cellWidth / 2),
                                                                     cell.position.y - (origin_.y + cell.home.y + cellHeight / 2)};
        }
    }

protected:
    // as large as the table and a margin (it takes what more it gets: its stretch)
    glw::Size measureContent(glw::Size available) override {
        return {std::min(available.width, columns * (cellWidth + gap) + 60), std::min(available.height, rows * (cellHeight + gap) + 60)};
    }

    void onMouse(glw::MouseEvent& event) override {
        if (event.type == glw::MouseEventType::Move)
            mouse_ = event.position;
    }

    void onHoverChanged(bool hovered) override {
        if (!hovered)
            mouse_.reset();
    }

    void paint(glw::PaintContext& context) override {
        paint::Painter& p = context.painter;
        p.setDepthTest(true);           // (fallen, they pass through one another)
        const glw::Theme& theme = context.ui.theme;
        const glw::Size size = this->size();
        origin_ = {(size.width - (columns * (cellWidth + gap) - gap)) / 2, (size.height - (rows * (cellHeight + gap) - gap)) / 2};
        // what the table lies on
        const glw::Rect table{origin_.x - 18, origin_.y - 18, columns * (cellWidth + gap) - gap + 36, rows * (cellHeight + gap) - gap + 36};
        p.fillRoundedRect(table, 12, glw::mix(theme.palette.layer, theme.palette.window, 0.4f));
        // glass: stripes of the series' colors under them, to show through them
        if (material_.glass) {
            p.pushClip(table);
            const auto& series = theme.chart.series;
            for (int k = 0; k * 26.0f < table.width + table.height; ++k) {
                const float x = table.x - table.height + static_cast<float>(k) * 26.0f;
                const glw::Point band[] = {{x, table.bottom()}, {x + 12, table.bottom()}, {x + 12 + table.height, table.y}, {x + table.height, table.y}};
                p.fillConvexPolygon(band, glw::withAlpha(series[static_cast<std::size_t>(k) % series.size()], 0.85f));
            }
            p.popClip();
        }
        // the bricks, the lowest first (those above come over them)
        order_.resize(cells_.size());
        for (std::size_t i = 0; i < cells_.size(); ++i)
            order_[i] = i;
        const auto height = [&](std::size_t i) { return cells_[i].falling ? cells_[i].position.z : cells_[i].lift.value; };
        std::sort(order_.begin(), order_.end(), [&](std::size_t a, std::size_t b) { return height(a) < height(b); });
        // their shadows on the table first, then the bricks: a shadow falls on what they lie on, not on another brick
        bricks_.clear();
        for (std::size_t i : order_) {
            const Cell& cell = cells_[i];
            paint::Brick brick;
            brick.halfSize = {cellWidth / 2, cellHeight / 2, thickness / 2};
            brick.cornerRadius = 7;
            brick.bevel = 0;            // faces flat to a sharp edge, its corners round
            const auto column = i % static_cast<std::size_t>(columns);
            brick.color = theme.chart.series[column % theme.chart.series.size()];
            brick.roughness = material_.roughness;
            brick.metal = material_.metal;
            brick.glass = material_.glass;
            if (brick.glass)
                brick.color = glw::mix(brick.color, Color{1, 1, 1, 1}, 0.4f);
            if (cell.falling) {
                brick.center = cell.position;
                brick.rotation = cell.turn;
            } else {
                Point offset{};
                if (const auto it = fly_.find(i); it != fly_.end())
                    offset = it->second;
                brick.center = {origin_.x + cell.home.x + cellWidth / 2 + offset.x, origin_.y + cell.home.y + cellHeight / 2 + offset.y,
                                thickness / 2 + cell.lift.value};
                brick.rotation = paint::Quaternion::tilt(cell.leanX.value, cell.leanY.value);
            }
            bricks_.push_back(brick);
        }
        for (const paint::Brick& brick : bricks_)
            p.drawBrickShadow(brick);
        for (std::size_t k = 0; k < order_.size(); ++k) {
            paint::Brick brick = bricks_[k];
            brick.shadow = 0;
            p.drawBrick(brick);
            // its name on it, in the middle of its face, of a color that reads on it
            const glw::text::TextLayout& name = label(context, order_[k]);
            const glw::Size extent = name.size();
            const float light = 0.299f * brick.color.r + 0.587f * brick.color.g + 0.114f * brick.color.b;
            const Color ink = light > 0.6f ? Color{0.08f, 0.09f, 0.11f, 0.9f} : Color{1, 1, 1, 0.95f};
            glw::text::drawTextOnBrick(p, context.ui.atlas, name, brick, {-extent.width / 2, -extent.height / 2}, ink);
        }
    }

private:
    // A cell's name (its column's letter, its row's number), laid out once.
    const glw::text::TextLayout& label(const glw::PaintContext& context, std::size_t i) {
        if (labels_.size() != cells_.size()) {
            labels_.clear();
            const glw::text::TextStyle style{{&context.ui.fonts.defaultFont()}, 15};
            for (std::size_t k = 0; k < cells_.size(); ++k) {
                const std::string name = std::string(1, static_cast<char>('A' + k % columns)) + std::to_string(k / columns + 1);
                labels_.emplace_back(name, style);
            }
        }
        return labels_[i];
    }

    void step() {
        const auto now = std::chrono::steady_clock::now();
        const float dt = std::min(0.033f, std::chrono::duration<float>(now - last_).count());
        last_ = now;
        if (dt <= 0)
            return;
        const float floor = size().height;
        bool moving = mouse_.has_value();
        for (std::size_t i = 0; i < cells_.size(); ++i) {
            Cell& cell = cells_[i];
            if (cell.falling) {
                // gravity toward the bottom of the window, a turn, a bounce off it
                cell.velocity.y += 1800 * dt;
                cell.velocity.z -= 300 * dt;
                cell.position = cell.position + cell.velocity * dt;
                if (cell.position.z < thickness / 2) {
                    cell.position.z = thickness / 2;
                    cell.velocity.z = -cell.velocity.z * 0.3f;
                }
                if (cell.position.y > floor - cellHeight / 2 && cell.velocity.y > 0) {
                    cell.position.y = floor - cellHeight / 2;
                    cell.velocity.y = -cell.velocity.y * 0.45f;
                    cell.velocity.x *= 0.7f;
                    cell.spin = cell.spin * 0.6f;
                }
                const float angle = length(cell.spin) * dt;
                if (angle > 0)
                    cell.turn = paint::Quaternion::axisAngle(cell.spin, angle) * cell.turn;
                moving = true;
                continue;
            }
            // flown back from where it fell
            if (const auto it = fly_.find(i); it != fly_.end()) {
                it->second.x *= std::exp(-7 * dt);
                it->second.y *= std::exp(-7 * dt);
                if (std::abs(it->second.x) + std::abs(it->second.y) < 0.2f)
                    fly_.erase(it);
                moving = true;
            }
            // near the mouse: risen and leaning toward it (only those near it: nothing spreads out of it)
            const Point middle{origin_.x + cell.home.x + cellWidth / 2, origin_.y + cell.home.y + cellHeight / 2};
            float lift = 0, leanX = 0, leanY = 0;
            if (mouse_) {
                const float dx = mouse_->x - middle.x, dy = mouse_->y - middle.y;
                const float near = std::exp(-(dx * dx + dy * dy) / (2 * 90.0f * 90.0f));
                lift = 26 * near;
                leanY = -0.45f * near * std::clamp(dx / 90.0f, -1.0f, 1.0f);
                leanX = 0.45f * near * std::clamp(dy / 90.0f, -1.0f, 1.0f);
            }
            cell.lift.step(lift, dt);
            cell.leanX.step(leanX, dt);
            cell.leanY.step(leanY, dt);
            moving = moving || std::abs(cell.lift.velocity) + std::abs(cell.lift.value) > 0.01f ||
                     std::abs(cell.leanX.value) + std::abs(cell.leanY.value) > 1e-4f;
        }
        if (moving)
            invalidatePaint();
    }

    std::vector<Cell> cells_;
    std::vector<glw::text::TextLayout> labels_;
    std::vector<std::size_t> order_;
    std::vector<paint::Brick> bricks_;
    std::map<std::size_t, Point> fly_;
    std::optional<Point> mouse_;
    Point origin_;
    Material material_ = materials[0];
    glw::Timer tick_;
    std::chrono::steady_clock::time_point last_;
    unsigned dropped_ = 0;
};

} // namespace

int main(int argc, char** argv) {
    int frames = -1;
    std::optional<glw::ThemeStyle> style;
    std::optional<glw::ColorScheme> colors;
    std::size_t material = 0;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--frames" && i + 1 < argc)
            frames = std::max(1, std::atoi(argv[++i]));
        else if (arg == "--style" && i + 1 < argc)
            style = glw::themeStyleNamed(argv[++i]);
        else if (arg == "--colors" && i + 1 < argc)
            colors = glw::colorSchemeNamed(argv[++i]);
        else if (arg == "--material" && i + 1 < argc) {
            const std::string_view m(argv[++i]);
            material = m == "glossy" ? 1 : m == "matte" ? 2 : m == "metal" ? 3 : m == "glass" ? 4 : 0;
        }
    }
    try {
        glw::Application app;
        if (style)
            app.setStyle(*style, false);
        if (colors)
            app.setColorScheme(*colors, false);
        auto board = std::make_unique<Board>();
        Board* bricks = board.get();
        bricks->setStretch(1);
        bricks->setMaterial(material);
        auto content = ui::VStack({.spacing = 10},
                                  ui::HStack({.spacing = 10},
                                             ui::Label({.text = "Material"}).alignment(glw::Alignment::Start, glw::Alignment::Center),
                                             ui::ComboBox({.items = {"Plastic", "Glossy", "Matte", "Metal", "Glass"},
                                                           .current = material,
                                                           .onChanged = [bricks](std::size_t i) { bricks->setMaterial(i); }}),
                                             ui::Button({.text = "Drop", .onClick = [bricks] { bricks->drop(); }}),
                                             ui::Button({.text = "Reset", .onClick = [bricks] { bricks->reset(); }}),
                                             ui::Label({.text = "Move the mouse over the bricks.", .muted = true})
                                                 .alignment(glw::Alignment::Start, glw::Alignment::Center)),
                                  std::unique_ptr<glw::Widget>(std::move(board)))
                           .padding(glw::Insets::all(12));
        app.openWindow({.title = "Bricks", .size = {1100, 720}}, std::move(content));
        bricks->start(*app.context().scheduler);
        return app.run(frames);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "brick_demo: %s\n", e.what());
        return 1;
    }
}
