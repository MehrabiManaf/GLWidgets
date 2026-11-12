// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// The Solid style, Plastic in three dimensions, and Glass, Plastic of glass. Everything they are lives here, as data the
// widgets already know: Solid's raised controls drawn as bricks (Painter::setReliefBricks, through Theme::reliefBricks),
// the camera they are seen from, docked panels arriving (DockStyle::landing, rising); Glass's reliefs of glass
// (Theme::glassReliefs). No widget asks for them by name.
#include <GLWidgets/Widgets/Theme.hpp>

#include <chrono>

namespace glw {

void Theme::solid(Theme& t, bool dark) {
    plastic(t, dark);
    // a little squarer than Plastic: its corners round over less, and the edges of its solids too (a solid's corner is no
    // rounder than its edge: Painter::drawBrick)
    t.cornerRadius = 3;
    t.button.cornerRadius = 3.75f;
    t.button.bevel.roundness = 2;
    t.toolBar.cornerRadius = t.menuBar.cornerRadius = t.menu.cornerRadius = 3;
    t.toolBar.bevel.roundness = 1.6f;
    t.collapsiblePanel.cornerRadius = 4.5f;
    t.collapsiblePanel.bevel.roundness = 2;
    t.lineEdit.cornerRadius = 3;
    t.lineEdit.bevel.roundness = 1.75f;
    t.checkBox.cornerRadius = 2.25f;
    t.checkBox.bevel.roundness = t.checkBox.checkedBevel.roundness = 1.25f;
    for (ItemViewStyle* view : {&t.itemView, &t.table}) {
        view->cornerRadius = 3;
        view->headerBevel.roundness = view->rowBevel.roundness = 1.6f;
    }
    t.dock.cornerRadius = t.card.cornerRadius = 3.75f;
    t.dock.tabRadius = 3;
    t.majorTab.cornerRadius = 4.5f;
    t.node.cornerRadius = 6;
    t.node.bevel.roundness = 2.75f;
    t.node.bodyBevel.roundness = 3;
    t.chart.barRadius = 2.25f;
    t.chart.barBevel.roundness = 1.25f;
    // menus, tooltips, dialogs: slabs over the window; a menu, a tooltip rising into its window, a dialog landing in it
    t.menu.bevel = t.toolTip.bevel = t.dialog.bevel = {.relief = 0.8f, .roundness = 1.5f};
    t.motion.popupEntrance = PopupEntrance::Rise;
    t.motion.dialogEntrance = PopupEntrance::Land;
    // a table's column headers keys of their own; the selection moving from row to row (a brick sliding)
    for (ItemViewStyle* view : {&t.itemView, &t.table}) {
        view->headerGap = 2;
        view->slideSelection = true;
    }
    // a collapsible panel's widgets coming out from under its header, one after another
    t.collapsiblePanel.cascade = true;
    // a check box's brick falling into its box as it is checked
    t.checkBox.checkDrop = 14;
    // a scroll bar's thumb a brick in a groove
    t.scrollBar.track = t.lineEdit.background.normal;
    t.scrollBar.trackBevel = {.relief = 1.2f, .roundness = 1.5f};
    t.scrollBar.thumb = t.button.background;
    t.scrollBar.thumbBevel = {.relief = 1.2f, .roundness = 1};
    // what stands on what it is on: its shadow under it a little darker
    const Color shadow{0, 0, 0, dark ? 0.6f : 0.28f};
    t.button.bevel.shadow = t.toolBar.bevel.shadow = t.collapsiblePanel.bevel.shadow = t.checkBox.checkedBevel.shadow = shadow;
    t.itemView.headerBevel.shadow = t.table.headerBevel.shadow = shadow;
    // its raised controls solids, thin: their bodies down into what they are on as deep as two and a half times Plastic's
    // reliefs are high; their tops flat to a sharp edge (only their corners round, as seen from above)
    t.reliefBricks = paint::ReliefBricks{.depth = 2.5f, .roughness = 0.32f, .bevel = 0};
    // seen from a little nearer than by default: their sides show toward the window's edges
    t.cameraDistance = 0.9f;
    // panels docked from a floating window fall into their places; floated, they rise into their window from behind it
    t.dock.landing = std::chrono::milliseconds(520);
    t.dock.rising = std::chrono::milliseconds(320);
}

void Theme::glass(Theme& t, bool dark) {
    // Plastic's reliefs, of glass: its controls, and the slabs they stand on (a stack's, a card's), showing what is under
    // them, the window's own
    plastic(t, dark);
    t.glassReliefs = paint::GlassReliefs{.from = 0.5f};
}

} // namespace glw
