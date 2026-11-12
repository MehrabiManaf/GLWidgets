// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// The Solid style and its Glass: Plastic in three dimensions. Everything they are lives here, as data the widgets already
// know: their raised controls drawn as bricks (Painter::setReliefBricks, through Theme::reliefBricks), of glass in Glass,
// the camera they are seen from, and docked panels arriving (DockStyle::landing, rising). No widget asks for them by name.
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
    // its raised controls solids, their bodies down into what they are on: as deep as five times Plastic's reliefs are
    // high, a little rougher than glass
    // their tops flat to a sharp edge (only their corners round, as seen from above)
    t.reliefBricks = paint::ReliefBricks{.depth = 5, .roughness = 0.32f, .bevel = 0};
    // seen from a little nearer than by default: their sides show toward the window's edges
    t.cameraDistance = 0.9f;
    // panels docked from a floating window fall into their places; floated, they rise into their window from behind it
    t.dock.landing = std::chrono::milliseconds(420);
    t.dock.rising = std::chrono::milliseconds(320);
}

void Theme::glass(Theme& t, bool dark) {
    solid(t, dark);
    // its controls panes of frosted glass, and the slabs they stand on (a stack's, a card's): what is under them, the
    // window's own, blurred through them
    t.reliefBricks->glass = true;
    t.reliefBricks->glassFrom = 0.5f;
}

} // namespace glw
