// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// What the widget tests show their trees with: one context (fonts, a glyph atlas only in memory, a translator, the dark
// theme) and a host that stands for a window.
#include <GLWidgets/Text/Font.hpp>
#include <GLWidgets/Text/GlyphAtlas.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <memory>
#include <string>
#include <vector>

namespace glw::tests {

// The time of the tests' scheduler: it moves only when a test moves it (advance).
inline TimePoint& testClock() {
    static TimePoint now{};
    return now;
}

// Shared by every test: a test that changes its language or theme puts them back. Its scheduler runs on testClock, its
// animations off (what eases shows where it goes at once) unless a test turns them on.
inline UiContext& testContext() {
    static text::FontLibrary fonts;
    static text::GlyphAtlas atlas(1, 256);
    static Translator translator;
    static Scheduler scheduler([] { return testClock(); });
    static const bool still = (scheduler.setAnimationsEnabled(false), true);
    static UiContext ui{fonts, atlas, translator, Theme::dark(), nullptr, &scheduler};
    (void)still;
    return ui;
}

inline Scheduler& testScheduler() {
    return *testContext().scheduler;
}

// The test's time moved on by `by`: the timers due fire, the animations step.
inline void advance(Duration by) {
    testClock() += by;
    testScheduler().tick();
}

inline Translator& testTranslator() {
    return const_cast<Translator&>(testContext().translator);
}

struct FakePopup;

// A window: its popups are roots of their own, kept here (opened in order, a submenu after its menu).
struct FakeHost : UiHost {
    platform::WindowState state = platform::WindowState::Normal;
    int minimized = 0, toggled = 0, closed = 0;
    std::vector<std::unique_ptr<FakePopup>> popups;     // the root host's: every popup, open or closed
    FakeHost* top = this;                               // the host of the window the popups came from
    UiRoot* own = nullptr;                              // a popup's root
    FakeHost* openedFrom = nullptr;                     // a popup's: the host it opened from
    bool gone = false;
    bool textInput = false;
    Rect inputArea;
    std::string clipboard;
    platform::Cursor cursor = platform::Cursor::Arrow;

    platform::WindowState windowState() const override { return state; }
    void minimizeWindow() override { ++minimized; }
    void toggleMaximizeWindow() override { ++toggled; }
    void closeWindow() override;
    void startTextInput() override { textInput = true; }
    void stopTextInput() override { textInput = false; }
    void setTextInputArea(const Rect& area) override { inputArea = area; }
    std::string clipboardText() const override { return clipboard; }
    void setClipboardText(const std::string& text) override { clipboard = text; }
    void setCursor(platform::Cursor shape) override { cursor = shape; }
    UiRoot* openPopup(PopupRequest request) override;

    // The popups open now, in order.
    std::vector<FakePopup*> open() const;
};

struct FakePopup {
    FakeHost host;
    UiRoot root;
    Rect anchor;
    PopupPlacement placement;
    bool modal;
    bool tooltip;

    FakePopup(const UiContext& context, FakeHost& from, PopupRequest request)
        : root(context, &host), anchor(request.anchor), placement(request.placement), modal(request.modal), tooltip(request.tooltip) {
        host.top = from.top;
        host.own = &root;
        host.openedFrom = &from;
        root.setContent(std::move(request.content));
        const Size size = root.content()->measure({unbounded, unbounded});
        root.setSize(size);
        root.layout();
    }
};

inline void FakeHost::closeWindow() {
    ++closed;
    if (!own || gone)
        return;
    // its own popups with it, then it
    gone = true;
    for (FakePopup* p : top->open()) {
        if (p->host.openedFrom == this)
            p->host.closeWindow();
    }
    own->windowClosing();
}

inline UiRoot* FakeHost::openPopup(PopupRequest request) {
    top->popups.push_back(std::make_unique<FakePopup>(testContext(), *this, std::move(request)));
    return &top->popups.back()->root;
}

inline std::vector<FakePopup*> FakeHost::open() const {
    std::vector<FakePopup*> result;
    for (const auto& p : top->popups) {
        if (!p->host.gone)
            result.push_back(p.get());
    }
    return result;
}

} // namespace glw::tests
