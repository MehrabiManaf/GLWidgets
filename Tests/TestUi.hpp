// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// What the widget tests show their trees with: one context (fonts, a glyph atlas only in memory, a translator, the dark
// theme) and a host that stands for a window.
#include <GLWidgets/Text/Font.hpp>
#include <GLWidgets/Text/GlyphAtlas.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <string>

namespace glw::tests {

// Shared by every test: a test that changes its language or theme puts them back.
inline UiContext& testContext() {
    static text::FontLibrary fonts;
    static text::GlyphAtlas atlas(1, 256);
    static Translator translator;
    static UiContext ui{fonts, atlas, translator, Theme::dark()};
    return ui;
}

inline Translator& testTranslator() {
    return const_cast<Translator&>(testContext().translator);
}

struct FakeHost : UiHost {
    platform::WindowState state = platform::WindowState::Normal;
    int minimized = 0, toggled = 0, closed = 0;
    bool textInput = false;
    Rect inputArea;
    std::string clipboard;

    platform::WindowState windowState() const override { return state; }
    void minimizeWindow() override { ++minimized; }
    void toggleMaximizeWindow() override { ++toggled; }
    void closeWindow() override { ++closed; }
    void startTextInput() override { textInput = true; }
    void stopTextInput() override { textInput = false; }
    void setTextInputArea(const Rect& area) override { inputArea = area; }
    std::string clipboardText() const override { return clipboard; }
    void setClipboardText(const std::string& text) override { clipboard = text; }
};

} // namespace glw::tests
