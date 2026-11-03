// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/Workspace.hpp>

#include "Widgets/Json.hpp"

#include <GLWidgets/Widgets/DockingHost.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>
#include <GLWidgets/Widgets/WindowFrame.hpp>

#include <cstdio>
#include <utility>
#include <vector>

namespace glw {

namespace {

// What a window shows: its frame's content, or its root's.
Widget* contentOf(const UiRoot& root) {
    Widget* content = root.content();
    if (auto* frame = dynamic_cast<WindowFrame*>(content))
        return frame->content();
    return content;
}

// The first dock area in `widget` (itself, or the first found going down).
DockArea* firstArea(Widget& widget) {
    if (auto* area = dynamic_cast<DockArea*>(&widget))
        return area;
    for (const auto& child : widget.children()) {
        if (DockArea* area = firstArea(*child))
            return area;
    }
    return nullptr;
}

void appendNumber(std::string& out, float value) {
    char text[32];
    std::snprintf(text, sizeof text, "%.9g", static_cast<double>(value));
    out += text;
}

void appendPair(std::string& out, float a, float b) {
    out += '[';
    appendNumber(out, a);
    out += ',';
    appendNumber(out, b);
    out += ']';
}

// "floating":[{"position":[x,y],"size":[w,h],"layout":{...}}, ...]
void appendFloating(std::string& out, const DockingHost& host, const DockArea& area) {
    out += "\"floating\":[";
    bool first = true;
    for (DockArea* floating : area.floatingAreas()) {
        const UiRoot* r = floating->root();
        if (!r)
            continue;
        if (!first)
            out += ',';
        first = false;
        const Point at = host.windowPosition(*r);
        out += "{\"position\":";
        appendPair(out, at.x, at.y);
        out += ",\"size\":";
        appendPair(out, r->size().width, r->size().height);
        out += ",\"layout\":";
        out += floating->saveLayout();
        out += '}';
    }
    out += ']';
}

float numberOf(const json::Value* value, float otherwise = 0) {
    const double* number = value ? std::get_if<double>(&value->data) : nullptr;
    return number ? static_cast<float>(*number) : otherwise;
}

// [a, b] as two numbers.
std::pair<float, float> pairOf(const json::Value* value, std::pair<float, float> otherwise) {
    const json::Array* array = value ? value->array() : nullptr;
    if (!array || array->size() != 2)
        return otherwise;
    return {numberOf(&(*array)[0], otherwise.first), numberOf(&(*array)[1], otherwise.second)};
}

const json::Array* arrayOf(const json::Value& object, std::string_view key) {
    const json::Value* value = object.member(key);
    return value ? value->array() : nullptr;
}

// The floating windows a record names, opened by `area` (of the group): their panels taken where they are.
void restoreFloating(DockArea& area, const json::Value& record) {
    const json::Array* floating = arrayOf(record, "floating");
    if (!floating)
        return;
    for (const json::Value& window : *floating) {
        const json::Value* layout = window.member("layout");
        if (!layout)
            continue;
        const auto [x, y] = pairOf(window.member("position"), {0.0f, 0.0f});
        const auto [width, height] = pairOf(window.member("size"), {320.0f, 240.0f});
        area.restoreFloating(json::write(*layout), {x, y}, {std::max(width, 120.0f), std::max(height, 80.0f)});
    }
}

} // namespace

std::string saveWorkspace(const DockingHost& host) {
    std::string out = "{\"version\":1,\"windows\":[";
    const DockArea* global = nullptr;      // the first window's: its group's floating windows are saved once
    bool firstWindow = true;
    for (const UiRoot* root : host.windows()) {
        Widget* content = contentOf(*root);
        auto* area = dynamic_cast<DockArea*>(content);
        if (area && area->floating())
            continue;       // with its group
        auto* tabs = dynamic_cast<MajorTabs*>(area ? area->center() : content);
        if (!tabs)
            continue;
        if (!firstWindow)
            out += ',';
        firstWindow = false;
        const Point at = host.windowPosition(*root);
        out += "{\"position\":";
        appendPair(out, at.x, at.y);
        out += ",\"size\":";
        appendPair(out, root->size().width, root->size().height);
        const bool maximized = root->host() && root->host()->windowState() == platform::WindowState::Maximized;
        out += maximized ? ",\"maximized\":true" : ",\"maximized\":false";
        if (area && area->scope() == DockScope::Global) {
            global = global ? global : area;
            out += ",\"panels\":";
            out += area->saveLayout();
        }
        // the major tabs with an id: their panels, their floating windows; the one shown among them
        out += ",\"tabs\":[";
        std::size_t saved = 0, active = 0;
        for (std::size_t i = 0; i < tabs->count(); ++i) {
            if (tabs->tabId(i).empty())
                continue;
            if (i == tabs->active())
                active = saved;
            if (saved++ > 0)
                out += ',';
            out += "{\"id\":";
            json::appendString(out, tabs->tabId(i));
            if (DockArea* panels = tabs->content(i) ? firstArea(*tabs->content(i)) : nullptr) {
                out += ",\"layout\":";
                out += panels->saveLayout();
                out += ',';
                appendFloating(out, host, *panels);
            }
            out += '}';
        }
        out += "],\"active\":";
        appendNumber(out, static_cast<float>(active));
        out += '}';
    }
    out += ']';
    if (global) {
        out += ',';
        appendFloating(out, host, *global);
    }
    out += '}';
    return out;
}

bool restoreWorkspace(DockingHost& host, std::string_view saved, const WorkspaceFactory& factory, std::string* error) {
    std::string message;
    const std::optional<json::Value> workspace = json::parse(saved, &message);
    const json::Array* windows = workspace ? arrayOf(*workspace, "windows") : nullptr;
    if (!windows) {
        if (error)
            *error = workspace ? "a workspace has its \"windows\"" : "not JSON: " + message;
        return false;
    }
    DockArea* global = nullptr;     // the first window's global area: the others are of its group
    // the global panels a layout names, made and put in `area`, for the layout to arrange
    auto makePanels = [&](DockArea& area, const std::string& layout) {
        if (!factory.panel)
            return;
        for (const std::string& id : DockArea::panelsIn(layout)) {
            if (area.tabsOf(id))
                continue;
            if (std::optional<DockPanel> panel = factory.panel(id); panel && panel->scope == DockScope::Global)
                area.addPanel(std::move(*panel));
        }
    };

    int opened = 0;
    for (const json::Value& window : *windows) {
        // its major tabs: the documents the application can make
        std::unique_ptr<MajorTabs> tabs = factory.tabs ? factory.tabs() : nullptr;
        if (!tabs)
            tabs = std::make_unique<MajorTabs>();
        std::vector<std::pair<std::size_t, const json::Value*>> restored;      // a tab's index, what was saved of it
        std::size_t active = 0;
        const json::Array* records = arrayOf(window, "tabs");
        const std::size_t activeSaved = static_cast<std::size_t>(std::max(0.0f, numberOf(window.member("active"))));
        for (std::size_t i = 0; records && i < records->size(); ++i) {
            const json::Value* id = (*records)[i].member("id");
            if (!id || !id->string() || !factory.document)
                continue;
            std::optional<WorkspaceDocument> document = factory.document(*id->string());
            if (!document)
                continue;
            const std::size_t index = tabs->addTab(std::move(document->title), std::move(document->content), document->closable);
            tabs->setTabId(index, *id->string());
            restored.emplace_back(index, &(*records)[i]);
            if (i <= activeSaved)
                active = index;
        }
        if (tabs->count() == 0)
            continue;
        tabs->setActive(active);
        MajorTabs* documents = tabs.get();
        std::unique_ptr<MajorTabStrip> strip = tabs->makeStrip();

        // around them, its global panels
        std::unique_ptr<Widget> content;
        if (const json::Value* panels = window.member("panels")) {
            std::unique_ptr<DockArea> area;
            if (global) {
                area = global->makeSibling(std::move(tabs));
            } else {
                area = std::make_unique<DockArea>(DockScope::Global);
                area->setCenter(std::move(tabs));
                global = area.get();
            }
            const std::string layout = json::write(*panels);
            makePanels(*area, layout);
            area->restoreLayout(layout);
            content = std::move(area);
        } else {
            content = std::move(tabs);
        }

        const auto [x, y] = pairOf(window.member("position"), {80.0f, 80.0f});
        const auto [width, height] = pairOf(window.member("size"), {960.0f, 640.0f});
        UiRoot& root = host.openWindow({.title = factory.title,
                                        .content = std::move(content),
                                        .titleCenter = std::move(strip),
                                        .position = {x, y},
                                        .size = {std::max(width, 200.0f), std::max(height, 120.0f)},
                                        .owner = nullptr});
        const json::Value* maximized = window.member("maximized");
        if (const bool* yes = maximized ? std::get_if<bool>(&maximized->data) : nullptr; yes && *yes && root.host())
            root.host()->toggleMaximizeWindow();

        // in a window now: each document's panels as they were, and their floating windows
        for (const auto& [index, record] : restored) {
            DockArea* area = documents->content(index) ? firstArea(*documents->content(index)) : nullptr;
            if (!area)
                continue;
            if (const json::Value* layout = record->member("layout"))
                area->restoreLayout(json::write(*layout));
            restoreFloating(*area, *record);
        }
        ++opened;
    }
    if (opened == 0) {
        if (error)
            *error = "no window of the workspace has a document that can be made";
        return false;
    }
    // the global floating windows: their panels made, then taken there
    if (const json::Array* floating = arrayOf(*workspace, "floating"); floating && global) {
        for (const json::Value& window : *floating) {
            if (const json::Value* layout = window.member("layout"))
                makePanels(*global, json::write(*layout));
        }
        restoreFloating(*global, *workspace);
    }
    return true;
}

} // namespace glw
