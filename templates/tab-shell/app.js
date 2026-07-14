// tab-shell — a bottom-tab app frame. Three screens (Home / Search / Profile)
// share a fixed bottom nav; tapping a tab switches the active frame.
//
// Make it yours: rename the frames + tabs, fill each screen's cards with real
// content, and bind data with ui.bindList (see the list-detail template).

ui.selectFrame(APP.entryFrame || "Home");

// The bottom nav stays put while a screen scrolls.
ui.findAll("Bottom Nav Bar").forEach((n) => (n.scrollFixed = true));

const TABS = { "tab-home": "Home", "tab-search": "Search", "tab-profile": "Profile" };
for (const [tab, frame] of Object.entries(TABS)) {
    ui.onClick(tab, () => ui.selectFrame(frame));
    ui.onHover(tab, (node, entered) => ui.setOpacity(node, entered ? 0.7 : -1.0));
}

console.log("tab-shell ready —", ui.frameNames().join(", "));
