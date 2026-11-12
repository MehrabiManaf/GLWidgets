// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// Dragging something from a widget to another in one window (between windows: DockingAcrossWindowsTests).
#include "Check.hpp"
#include "TestUi.hpp"

#include <GLWidgets/Widgets/DragDrop.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>
#include <GLWidgets/Widgets/Widget.hpp>

#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace glw;

namespace {

// Pressed and moved 5 dips, it drags its name.
class Source final : public Widget {
public:
    std::vector<MouseEventType> told;       // what it was told of the mouse
    std::vector<DropAction> finished;
    bool started = false;

protected:
    void onMouse(MouseEvent& e) override {
        told.push_back(e.type);
        e.handled = true;
        if (e.type == MouseEventType::Press)
            pressed_ = e.position;
        else if (e.type == MouseEventType::Release)
            pressed_.reset();
        else if (e.type == MouseEventType::Move && pressed_ && std::hypot(e.position.x - pressed_->x, e.position.y - pressed_->y) > 4) {
            pressed_.reset();
            started = root()->startDrag({.data = {"test/name", "A"}, .finished = [this](DropAction a) { finished.push_back(a); }});
        }
    }

private:
    std::optional<Point> pressed_;
};

// Takes "test/name" (a copy; with Ctrl, a move), and keeps what was dropped where.
class Target final : public Widget {
public:
    std::string takes = "test/name";
    std::vector<std::string> dropped;
    Point droppedAt;
    int overs = 0, leaves = 0;
    bool over = false;

protected:
    DropAction onDragOver(const DragEvent& e) override {
        if (e.data.format != takes)
            return DropAction::None;
        ++overs;
        over = true;
        return platform::hasAny(e.modifiers, platform::KeyModifiers::Control) ? DropAction::Move : DropAction::Copy;
    }
    void onDragLeave() override {
        ++leaves;
        over = false;
    }
    DropAction onDrop(const DragEvent& e) override {
        dropped.push_back(e.data.content);
        droppedAt = e.position;
        return platform::hasAny(e.modifiers, platform::KeyModifiers::Control) ? DropAction::Move : DropAction::Copy;
    }
};

// A child of a target, that takes nothing itself: its parent is asked.
class Plain final : public Widget {};

// The source on the left (0..100), the target on the right (200..400) with a plain child (250..300), nothing between.
class Board final : public Widget {
public:
    Source* source;
    Target* target;
    Plain* plain;
    Board() {
        source = static_cast<Source*>(&addChild(std::make_unique<Source>()));
        target = static_cast<Target*>(&addChild(std::make_unique<Target>()));
        plain = static_cast<Plain*>(&target->addChild(std::make_unique<Plain>()));
    }

protected:
    void arrangeContent(const Rect&) override {
        source->arrange({0, 0, 100, 100});
        if (target) {
            target->arrange({200, 0, 200, 100});
            plain->arrange({50, 0, 50, 50});
        }
    }
};

struct Window {
    tests::FakeHost host;
    UiRoot root{tests::testContext(), &host};
    Board* board;
    Window() {
        auto b = std::make_unique<Board>();
        board = b.get();
        root.setContent(std::move(b));
        root.setSize({400, 100});
        root.layout();
    }
    void press(Point p) { root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, true, 1, p}); }
    void move(Point p) { root.dispatch(platform::MouseMoved{1, p}); }
    void release(Point p) { root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, false, 1, p}); }
    void key(platform::Key k, platform::KeyModifiers m = platform::KeyModifiers::None, bool pressed = true) {
        root.dispatch(platform::KeyChanged{1, k, m, pressed, false});
    }
};

} // namespace

TEST(aDragGoesWhereItIsTakenAndTellsItsSource) {
    Window w;
    Source& source = *w.board->source;
    Target& target = *w.board->target;
    w.press({10, 10});
    w.move({20, 10});
    CHECK(source.started && w.root.dragging());
    CHECK(w.host.cursor == platform::Cursor::NotAllowed);       // over the source: not taken there
    w.move({150, 10});
    CHECK(w.host.cursor == platform::Cursor::NotAllowed);       // between them
    w.move({220, 10});
    CHECK(target.over && w.host.cursor == platform::Cursor::DragCopy);    // over it: copied there
    w.move({260, 10});                                          // over its plain child: its parent takes it
    CHECK(target.over && target.leaves == 0);
    w.move({150, 10});
    CHECK(!target.over && target.leaves == 1);
    w.move({230, 40});
    w.release({230, 40});
    CHECK(!w.root.dragging());
    CHECK((target.dropped == std::vector<std::string>{"A"}) && target.droppedAt == Point{30, 40});
    CHECK(!target.over);
    CHECK((source.finished == std::vector<DropAction>{DropAction::Copy}));
    // the source was told of the press and the moves until it started, not of the release
    CHECK(source.told.back() == MouseEventType::Move);
    CHECK(w.host.cursor == platform::Cursor::Arrow);
}

TEST(aDragLetGoWhereNothingTakesItDoesNothing) {
    Window w;
    w.board->target->takes = "test/other";
    w.press({10, 10});
    w.move({20, 10});
    w.move({220, 10});
    CHECK(w.host.cursor == platform::Cursor::NotAllowed);
    w.release({220, 10});
    CHECK(w.board->target->dropped.empty());
    CHECK((w.board->source->finished == std::vector<DropAction>{DropAction::None}));
}

TEST(escapeCancelsADragAndModifiersChangeWhatItDoes) {
    Window w;
    Target& target = *w.board->target;
    w.press({10, 10});
    w.move({20, 10});
    w.move({220, 10});
    const int overs = target.overs;
    w.key(platform::Key::Control, platform::KeyModifiers::Control);   // asked again, with Ctrl
    CHECK(target.overs == overs + 1);
    w.key(platform::Key::Escape, platform::KeyModifiers::Control);
    CHECK(!w.root.dragging() && !target.over);
    CHECK((w.board->source->finished == std::vector<DropAction>{DropAction::None}));
    w.release({220, 10});                                           // let go after: nothing more
    CHECK(target.dropped.empty() && w.board->source->finished.size() == 1);
    // again, dropped with Ctrl: a move
    w.press({10, 10});
    w.move({20, 10});
    w.move({220, 10});
    w.release({220, 10});
    CHECK(target.dropped.size() == 1 && w.board->source->finished.back() == DropAction::Move);
}

TEST(aTargetThatGoesDuringADragIsForgotten) {
    Window w;
    w.press({10, 10});
    w.move({20, 10});
    w.move({220, 10});
    CHECK(w.board->target->over);
    auto gone = w.board->removeChild(*std::exchange(w.board->target, nullptr));
    w.root.layout();
    w.move({230, 10});
    w.release({230, 10});
    CHECK((w.board->source->finished == std::vector<DropAction>{DropAction::None}));
}
