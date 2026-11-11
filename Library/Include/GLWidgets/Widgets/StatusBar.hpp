// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// A line along the bottom of a window (Window::setStatusBar): a message at its start side, for a while or until another
// (what was saved, what was found), and widgets that stay at its end side (the state of a run, a zoom). Without a message,
// its steady text shows (setText: "Ready").
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/Animation.hpp>
#include <GLWidgets/Widgets/Localization.hpp>
#include <GLWidgets/Widgets/Widget.hpp>

#include <chrono>
#include <memory>
#include <optional>

namespace glw {

class Label;

class GLW_EXPORT StatusBar : public Widget {
public:
    StatusBar();

    // `text` at its start side for `timeout` (zero: until another, or clearMessage).
    void showMessage(LocalizedText text, std::chrono::milliseconds timeout = std::chrono::milliseconds::zero());
    void clearMessage();
    // The message shown, if one is.
    const std::optional<LocalizedText>& message() const { return message_; }
    // What shows while there is no message.
    void setText(LocalizedText text);

    // A widget at its end side, after those added before (before them in right to left: at the end side still).
    template <class T>
    T& addPermanent(std::unique_ptr<T> widget) {
        T& added = *widget;
        addPermanentWidget(std::move(widget));
        return added;
    }

protected:
    void paint(PaintContext& context) override;

private:
    void addPermanentWidget(std::unique_ptr<Widget> widget);
    void shown();           // the message, or the steady text

    Label* label_ = nullptr;
    std::optional<LocalizedText> message_;
    LocalizedText text_;
    Timer timeout_;
};

} // namespace glw
