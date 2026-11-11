// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include "Widgets/TabPaint.hpp"

#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/Bevel.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>
#include <GLWidgets/Widgets/Widget.hpp>

#include <algorithm>

namespace glw::tabs {

void paintTab(PaintContext& context, const DockStyle& s, Size size, bool active, const VisualState& state) {
    paint::Painter& p = context.painter;
    const Rect r{0, 0, size.width, size.height};
    if (!s.tabsJoined) {
        // standing on the bar as a control: the one shown raised, one under the mouse lighter (raised too)
        const Rect key = r.inflated(-3);
        const float radius = std::min(s.tabRadius, key.height / 2);
        if (active)
            paintRaised(p, key, radius, s.selectedTab, {0, 0, 0, 0}, s.tabBevel, state.pressed);
        else if (state.hovered && s.tabBevel.relief > 0)
            paintRaised(p, key, radius, s.tab.resolve(state), {0, 0, 0, 0}, s.tabBevel, state.pressed);
        else
            p.fillRoundedRect(key, radius, s.tab.resolve(state));
    } else if (active) {
        // as Fusion's: raised over the bar, lit at its top, its edge around it but under it, where it joins its content
        // (its lower corners and edge below the bar, cut)
        const Rect shape{0, selectedTabTop, r.width, r.height - selectedTabTop + std::max(s.tabRadius, 2.0f)};
        const float radius = s.tabRadius;
        if (s.islandBevel.relief > 0)
            return;         // a raised stack's: the stack draws it, joined to its slab
        if (s.tabBevel.relief > 0) {
            // a relief of its content's color, its lower edge under the bar (cut): its face the content's
            const Rect taller{shape.x, shape.y, shape.width, shape.height + s.tabBevel.roundness + 2};
            p.pushClip(r);
            p.fillRelief(taller, radius, s.tabBevel.relief, s.tabBevel.roundness, s.selectedTab);
            p.popClip();
            p.fillRect({1, r.height - 1, r.width - 2, 1}, s.background);
            return;
        }
        p.pushClip(r);
        p.fillRoundedRect(shape, radius, s.selectedTab, s.background);
        p.strokeRoundedRect(shape.inflated(-1), std::max(0.0f, radius - 1), 1, s.selectedTabHighlight, withAlpha(s.selectedTabHighlight, 0));
        p.strokeRoundedRect(shape, radius, 1, s.selectedTabBorder);
        p.popClip();
        p.fillRect({1, r.height - 1, r.width - 2, 1}, s.background);     // the hairline under the bar open under it
    } else if (state.hovered && s.tabBevel.relief > 0) {
        const Rect key = r.inflated(-3);
        paintRaised(p, key, std::min(s.tabRadius, key.height / 2), s.tab.resolve(state), {0, 0, 0, 0}, s.tabBevel, state.pressed);
    } else {
        p.fillRoundedRect(r.inflated(-3), s.cornerRadius, s.tab.resolve(state));
    }
}

void paintClose(PaintContext& context, const DockStyle& s, const Rect& area, bool hovered) {
    paint::Painter& p = context.painter;
    if (hovered)
        p.fillRoundedRect(area, s.cornerRadius, context.ui.theme.palette.hover);
    const Point m = area.center();
    p.drawLine({m.x - 4, m.y - 4}, {m.x + 4, m.y + 4}, 1.2f, s.text);
    p.drawLine({m.x - 4, m.y + 4}, {m.x + 4, m.y - 4}, 1.2f, s.text);
}

void paintStack(PaintContext& context, const DockStyle& s, Size size, float bar, std::optional<Rect> activeTab, const Rect& row) {
    paint::Painter& p = context.painter;
    const Rect r{0, 0, size.width, size.height};
    if (s.islandBevel.relief > 0) {
        // raised: a slab of its background under its bar, the tab shown a relief as high joined to it, its lower part
        // over the slab's top edge (cut there): a folder's tab, its face the slab's
        const Bevel& slab = s.islandBevel;
        paintRaised(p, {0, bar, r.width, std::max(0.0f, r.height - bar)}, s.cornerRadius, s.background, {0, 0, 0, 0}, slab);
        if (bar >= 1 && activeTab) {
            const Rect& tab = *activeTab;
            const float top = std::max(0.0f, tab.y + selectedTabTop);
            const Rect shape{tab.x, top, tab.width, bar - top + slab.roundness + s.tabBevel.roundness + 2};
            // within the row (scrolled), down to the slab's edge
            const float left = std::max(tab.x, row.x) - 1, right = std::min(tab.x + tab.width, row.x + row.width) + 1;
            if (right > left) {
                p.pushClip({left, 0, right - left, bar + slab.roundness});
                p.fillRelief(shape, s.tabRadius, slab.relief, s.tabBevel.roundness, s.background);
                p.popClip();
            }
        }
        return;
    }
    p.fillRoundedRect(r, s.cornerRadius, s.background);
    p.strokeRoundedRect(r, s.cornerRadius, 1, s.border);
    if (bar >= 1)
        p.fillRect({s.cornerRadius / 2, bar - 1, std::max(0.0f, r.width - s.cornerRadius), 1}, s.separator);
}

} // namespace glw::tabs
