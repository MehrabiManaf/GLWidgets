// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// A workspace saved and restored, as JSON: the windows of the application (where they are, how large, maximized or not),
// the major tabs of each with the arrangement of their panels and their floating windows, and the global panels of each
// window and their floating windows.
//
// What is saved is the arrangement, not the content: restoring asks the application for the content by the ids it saved
// (a window's MajorTabs with its handlers, a document by its tab's id, a global panel by its id) and puts each where it
// was. A document's own panels are made with it: the saved layout arranges them.
//
//   app.setOnQuitting([&] { write(path, glw::saveWorkspace(app.docking())); });
//   if (!glw::restoreWorkspace(app.docking(), read(path), factory))
//       ...the windows of a first start...
//
// A window is saved when what its frame shows is a MajorTabs, or a Global DockArea around one (DockArea::setCenter). A
// major tab is saved when it has an id (MajorTabs::setTabId); its panels are those of the first DockArea in its content.
// Screen positions are saved as they are: a window of a screen no longer there is restored off it.
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/Docking.hpp>
#include <GLWidgets/Widgets/Localization.hpp>
#include <GLWidgets/Widgets/MajorTabs.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace glw {

class DockingHost;

// A document to show in a major tab.
struct WorkspaceDocument {
    LocalizedText title;
    std::unique_ptr<Widget> content;
    bool closable = true;
};

// What restoring a workspace asks the application for.
struct WorkspaceFactory {
    LocalizedText title;                    // of the windows
    // A window's major tabs, without tabs, with their handlers. None: a MajorTabs without handlers.
    std::function<std::unique_ptr<MajorTabs>()> tabs;
    // The document of a major tab saved with `id`. None: it is left out.
    std::function<std::optional<WorkspaceDocument>(const std::string& id)> document;
    // The global panel `id` (of the Global scope). None: it is left out.
    std::function<std::optional<DockPanel>(const std::string& id)> panel;
};

GLW_EXPORT std::string saveWorkspace(const DockingHost& host);
// Opens the windows `saved` describes. False (and `error`), with nothing opened, when it is not a workspace or none of its
// windows has a document that can be made.
GLW_EXPORT bool restoreWorkspace(DockingHost& host, std::string_view saved, const WorkspaceFactory& factory,
                                 std::string* error = nullptr);

} // namespace glw
