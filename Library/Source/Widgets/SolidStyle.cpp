// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// The Solid style, Plastic in three dimensions, and Glass, Plastic of glass. Everything they are lives here, as data the
// widgets already know: Solid's raised controls drawn as bricks (Painter::setReliefBricks, through Theme::reliefBricks),
// the camera they are seen from, docked panels arriving (DockStyle::landing, rising); Glass's reliefs of glass
// (Theme::glassReliefs). No widget asks for them by name.
#include <GLWidgets/Widgets/Theme.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>

namespace glw {

void Theme::solid(Theme& t, bool dark) {
    plastic(t, dark);
    // its corners a little rounder than they were at first, about as round as Plastic's; the edges of its solids round
    // over less (a solid's corner is no rounder than its edge: Painter::drawBrick)
    t.cornerRadius = 4;
    t.button.cornerRadius = 5;
    t.button.bevel.roundness = 2;
    t.toolBar.cornerRadius = t.menuBar.cornerRadius = t.menu.cornerRadius = 4;
    t.toolBar.bevel.roundness = 1.6f;
    t.collapsiblePanel.cornerRadius = 6;
    t.collapsiblePanel.bevel.roundness = 2;
    t.lineEdit.cornerRadius = 4;
    t.lineEdit.bevel.roundness = 1.75f;
    t.checkBox.cornerRadius = 3;
    t.checkBox.bevel.roundness = t.checkBox.checkedBevel.roundness = 1.25f;
    for (ItemViewStyle* view : {&t.itemView, &t.table}) {
        view->cornerRadius = 4;
        view->headerBevel.roundness = view->rowBevel.roundness = 1.6f;
    }
    t.dock.cornerRadius = t.card.cornerRadius = 5;
    t.dock.tabRadius = 4;
    t.majorTab.cornerRadius = 6;
    t.node.cornerRadius = 8;
    t.node.bevel.roundness = 2.75f;
    t.node.bodyBevel.roundness = 3;
    t.chart.barRadius = 3;
    t.chart.barBevel.roundness = 1.25f;
    // menus, tooltips, dialogs: slabs over the window; a menu, a tooltip rising into its window, a dialog landing in it
    t.menu.bevel = t.toolTip.bevel = t.dialog.bevel = {.relief = 0.8f, .roundness = 1.5f, .edge = 2};
    // ... a menu's item under the mouse a key raised on its slab
    t.menu.highlightBevel = {.shadow = {0, 0, 0, dark ? 0.5f : 0.22f}, .relief = 1.2f, .roundness = 1.5f};
    t.motion.popupEntrance = PopupEntrance::Rise;
    t.motion.dialogEntrance = PopupEntrance::Land;
    // and each time one of their ways, at random: a popup's, a dialog's, a panel's cascade, a check box's mark
    t.motion.varied = true;
    // a table's column headers keys of their own; the selection moving from row to row (a brick sliding)
    for (ItemViewStyle* view : {&t.itemView, &t.table}) {
        view->headerGap = 2;
        view->slideSelection = true;
        view->flipSorted = true;            // a header popping up and rocking as its order changes
        // the rows' highlights keys raised on the view: solid colors (not tints of what is under them), the one selected
        // of the accent, the one under the mouse a step above the view, down as it is pressed
        const Palette& p = t.palette;
        view->selected = mix(p.layer, p.accent, dark ? 0.78f : 0.55f);
        view->row = {.normal = {0, 0, 0, 0}, .hovered = mix(p.layer, p.surface, 0.75f), .pressed = mix(p.layer, p.surface, 0.45f)};
    }
    // a collapsible panel's widgets coming out from under its header, one after another
    t.collapsiblePanel.cascade = true;
    // a spin box's arrows keys in its field; a splitter's handle a groove (a bar raised out of it as it is taken); the
    // status bar a groove along the bottom of the window
    t.lineEdit.stepperBevel = {.relief = 1.0f, .roundness = 1};
    t.splitter.grooveWidth = 3;
    t.splitter.groove = {.relief = 0.8f, .roundness = 1};
    t.statusBar = {.bevel = {.relief = 1.0f, .roundness = 1.5f}, .inset = 2};
    // a node lifted while it is dragged
    t.node.dragLift = 10;
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
    t.itemView.rowBevel.shadow = t.table.rowBevel.shadow = shadow;
    // its raised controls solids, thin: their bodies down into what they are on as deep as two and a half times Plastic's
    // reliefs are high; their tops flat to a sharp edge (only their corners round, as seen from above)
    t.reliefBricks = paint::ReliefBricks{.depth = 2.5f, .roughness = 0.32f, .bevel = 0};
    // seen from a little nearer than by default: their sides show toward the window's edges
    t.cameraDistance = 0.9f;
    // panels docked from a floating window fall into their places; floated, they rise into their window from behind it
    t.dock.landing = std::chrono::milliseconds(750);
    t.dock.rising = std::chrono::milliseconds(500);
    t.dock.variedArrivals = true;           // each one of three ways, at random
    // a chart's bars, tiles and slices (solids) coming in a little slower than the rest: one after another, in space
    t.chart.entrance = std::chrono::milliseconds(1100);
}

void Theme::glass(Theme& t, bool dark) {
    // Plastic's reliefs, of glass: its controls, and the slabs they stand on (a stack's, a card's), showing what is under
    // them, the window's own
    plastic(t, dark);
    t.glassReliefs = paint::GlassReliefs{.from = 0.5f, .glossy = true};
    // the windows' background pools of color and sheens of light (around the accent's hue, a little away from it), for
    // the glass to show something
    const Color a = t.palette.accent;
    const float high = std::max({a.r, a.g, a.b}), low = std::min({a.r, a.g, a.b});
    float hue = 0;
    if (high > low) {
        if (high == a.r)
            hue = std::fmod((a.g - a.b) / (high - low) / 6 + 1, 1.0f);
        else if (high == a.g)
            hue = ((a.b - a.r) / (high - low) + 2) / 6;
        else
            hue = ((a.r - a.g) / (high - low) + 4) / 6;
    }
    t.backdrop = paint::Backdrop{.base = t.frame.background, .hue = std::fmod(hue + 0.55f, 1.0f), .vivid = dark ? 1.0f : 0.8f};
}

} // namespace glw
