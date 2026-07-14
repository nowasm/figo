// calendar — benchmark app: month grid (one 42-cell bindList in a WRAP
// stack) + day agenda + slideUp event composer with the alarm wheel recipe.
//
// Engine notes:
// - G14 interaction: setVisible on an auto-layout FLOW child collapses its
//   slot. The 42 grid cells are always-visible flow children; everything
//   that toggles (dot / today ring / selection disc / number pair) lives
//   INSIDE a plain (non-auto-layout) cell frame in absolute positions, so
//   visibility flips never reshape the grid.
// - bindList re-hugs the grid's primary axis using the WRAP container's
//   no-wrap extent; the design pins minWidth=maxWidth=371 so the clamp
//   restores the 7-column wrap (see gen_design.py).
// - Structural mutations (bindList) are illegal inside event dispatch:
//   handlers mutate state and defer render() via setTimeout(0).
// - Wheels: bound once (+2 phantom tail rows, G15), snapToChildren set after
//   binding, onScrollEnd drives the HH:MM preview, snapTo(...,0) = instant.

ui.selectFrame(APP.entryFrame || "Month");

const STORE_KEY = "calendar.events.v1";

// Deterministic bench start: the storage file survives between runs.
if (globalThis.SELFDRIVE) localStorage.removeItem(STORE_KEY);

const NOW = new Date();
const YEAR = NOW.getFullYear();
const MONTH = NOW.getMonth();               // 0-based
const TODAY = NOW.getDate();
const DAYS = new Date(YEAR, MONTH + 1, 0).getDate();
const OFFSET = (new Date(YEAR, MONTH, 1).getDay() + 6) % 7;  // Monday-first
const CELLS = 42;
const MONTH_TAG = `${YEAR}-${MONTH + 1}`;

const pad2 = v => String(v).padStart(2, "0");
const hhmm = (h, m) => pad2(h) + ":" + pad2(m);

// Presets on fixed days (all <= 28: valid in any month, spread across weeks).
const PRESETS = [
    { d: 3, h: 10, m: 0, title: "组会" },
    { d: 10, h: 14, m: 30, title: "牙医" },
    { d: 17, h: 19, m: 0, title: "健身" },
    { d: 24, h: 20, m: 0, title: "读书会" },
];

// ---- state (re-read on hot reload -> re-entrant) ----
let events = null;
try {
    const saved = JSON.parse(localStorage.getItem(STORE_KEY) || "null");
    if (saved && saved.month === MONTH_TAG && Array.isArray(saved.events))
        events = saved.events;
} catch (e) { /* corrupt storage -> presets */ }
if (!events) events = PRESETS.map(e => Object.assign({}, e));

function save() {
    localStorage.setItem(STORE_KEY,
                         JSON.stringify({ month: MONTH_TAG, events }));
}

let selected = TODAY;

const dayEvents = d => events.filter(e => e.d === d)
    .sort((a, b) => (a.h * 60 + a.m) - (b.h * 60 + b.m));

// ---- rendering (whole Month page from state; deferred from handlers) ----
function render() {
    ui.bindList("MonthGrid", CELLS, (cell, i) => {
        const d = i - OFFSET + 1;
        const inMonth = d >= 1 && d <= DAYS;
        const isSel = inMonth && d === selected;
        cell.find("DayNum").text = inMonth ? String(d) : "";
        cell.find("DayNumSel").text = inMonth ? String(d) : "";
        cell.find("DayNum").visible = inMonth && !isSel;
        cell.find("DayNumSel").visible = isSel;
        cell.find("SelBg").visible = isSel;
        cell.find("TodayRing").visible = inMonth && d === TODAY;
        cell.find("Dot").visible = inMonth && dayEvents(d).length > 0;
    });
    const list = dayEvents(selected);
    ui.bindList("AgendaList", list.length, (row, i) => {
        row.find("EvTime").text = hhmm(list[i].h, list[i].m);
        row.find("EvTitle").text = list[i].title;
    });
    ui.setText("AgendaTitle", `${MONTH + 1}月${selected}日`);
    ui.setText("AgendaCount", list.length ? `${list.length} 项日程` : "");
    ui.setVisible("EmptyState", list.length === 0);
}
render();
ui.setText("MonthTitle", `${YEAR}年${MONTH + 1}月`);

ui.onClick("DayCell", (node) => {
    const d = node.index - OFFSET + 1;
    if (d < 1 || d > DAYS || d === selected) return;
    selected = d;
    setTimeout(render, 0);  // bindList is illegal inside event dispatch
});

// ---- wheels (bound once; +2 phantom tail rows each, G15) ----
const WHEEL_ROW = 56;
const HOURS = 24;
const MINUTES = [0, 15, 30, 45];
ui.bindList("HourWheel", HOURS + 2,
    (item, i) => { item.find("HourText").text = i < HOURS ? pad2(i) : ""; });
ui.bindList("MinuteWheel", MINUTES.length + 2, (item, i) => {
    item.find("MinuteText").text = i < MINUTES.length ? pad2(MINUTES[i]) : "";
});
ui.find("HourWheel").snapToChildren = true;
ui.find("MinuteWheel").snapToChildren = true;

let pending = { h: 9, m: 0 };
function preview() { ui.setText("PreviewTime", hhmm(pending.h, pending.m)); }

ui.onScrollEnd("HourWheel", (n, x, y, idx) => {
    if (idx >= 0) { pending.h = Math.min(idx, HOURS - 1); preview(); }
});
ui.onScrollEnd("MinuteWheel", (n, x, y, idx) => {
    if (idx >= 0) {
        pending.m = MINUTES[Math.min(idx, MINUTES.length - 1)];
        preview();
    }
});

// ---- composer ----
ui.setEditable("TitleInput");

ui.onClick("AddBtn", () => {
    pending = { h: 9, m: 0 };
    ui.setText("TitleInput", "");
    preview();
    ui.navigateTo("NewEvent", "slideUp", 0.3);
    ui.snapTo("HourWheel", pending.h, 0);   // instant; fires onScrollEnd
    ui.snapTo("MinuteWheel", 0, 0);
});
ui.onClick("InputBox", () => ui.focusText("TitleInput"));
ui.onClick("BtnCancel", () => { ui.blur(); ui.navigateBack(0.3); });
ui.onClick("BtnSave", () => {
    const title = (ui.find("TitleInput").text || "")
        .replace(/\n/g, " ").trim() || "未命名日程";
    ui.blur();
    events.push({ d: selected, h: pending.h, m: pending.m, title });
    save();
    setTimeout(render, 0);
    ui.navigateBack(0.3);
});

// placeholder follows the live edit buffer every frame
ui.onUpdate(() => {
    const t = ui.find("TitleInput");
    if (t) ui.setVisible("TitlePlaceholder", (t.text || "").length === 0);
});

// press feedback (plain frames, not component instances)
for (const n of ["AddBtn", "BtnCancel", "BtnSave", "EvRow"])
    ui.onHover(n, (node, entered) => ui.setOpacity(node, entered ? 0.8 : -1.0));

console.log("calendar ready");

// ---- unattended tour (figoplay --selfdrive) ----
// shots: frame 30 -> <prefix>_home.png (pristine Month, today selected),
//        frame 110 -> <prefix>_nav.png (Month after saving the new event),
//        host exits at frame 140.
if (globalThis.SELFDRIVE) {
    const checks = [];
    const chk = (what, got, want) => checks.push([what, String(got), String(want)]);
    const near = (v, want, tol) => Math.abs(v - want) <= tol;
    const cells = () => ui.findAll("DayCell");
    const cellOf = d => cells()[OFFSET + d - 1];
    const dotCount = () => cells().filter(c => c.find("Dot").visible).length;
    const agendaRows = () => ui.findAll("EvRow");
    const EVT_DAY = 10;    // preset 牙医 14:30
    const EMPTY_DAY = 5;   // never a preset day; gets the new event
    const NEW_TITLE = "复盘会议";

    let frames = 0;  // count frames, not dt: first dt includes file load
    ui.onUpdate(() => {
        frames++;
        if (frames === 4) {
            chk("grid has 42 cells", ui.find("MonthGrid").childCount, CELLS);
            chk("grid wraps to 7 cols", ui.find("MonthGrid").width, 371);
            chk("month title", ui.find("MonthTitle").text,
                `${YEAR}年${MONTH + 1}月`);
            chk("today ring", cellOf(TODAY).find("TodayRing").visible, "true");
            chk("today selected", cellOf(TODAY).find("SelBg").visible, "true");
            chk("preset dots", dotCount(), PRESETS.length);
            chk("agenda header", ui.find("AgendaTitle").text,
                `${MONTH + 1}月${TODAY}日`);
            // wheels laid out even off-frame: extent must reach the last value
            chk("hour maxScrollY",
                near(ui.find("HourWheel").maxScrollY, 23 * WHEEL_ROW, 1), "true");
            chk("minute maxScrollY",
                near(ui.find("MinuteWheel").maxScrollY, 3 * WHEEL_ROW, 1), "true");
            chk("diagnostics clean", ui.diagnostics().length, 0);
        }
        if (frames === 6) chk("tap event day", ui.tap(cellOf(EVT_DAY)), "true");
        if (frames === 10) {
            chk("event day selected", cellOf(EVT_DAY).find("SelBg").visible, "true");
            chk("agenda 1 row", agendaRows().length, 1);
            chk("agenda time", agendaRows()[0].find("EvTime").text, "14:30");
            chk("agenda title", agendaRows()[0].find("EvTitle").text, "牙医");
            chk("empty state hidden", ui.find("EmptyState").visible, "false");
            ui.tap(cellOf(EMPTY_DAY));
        }
        if (frames === 14) {
            chk("empty day agenda", agendaRows().length, 0);
            chk("empty state shown", ui.find("EmptyState").visible, "true");
            ui.tap(cellOf(TODAY));  // restore pristine state for the home shot
        }
        if (frames === 18)
            chk("back to today", ui.find("AgendaTitle").text,
                `${MONTH + 1}月${TODAY}日`);

        // frames 18..30: untouched Month for the _home shot
        if (frames === 32) ui.tap(cellOf(EMPTY_DAY));  // target day for the event
        if (frames === 36) chk("tap add", ui.tap("AddBtn"), "true");
        if (frames === 60) {  // slideUp 0.3s long settled
            chk("composer settled", ui.transitionProgress() >= 1, "true");
            chk("on composer", ui.currentFrame().name, "NewEvent");
            chk("title starts empty", ui.find("TitleInput").text, "");
            chk("placeholder shown", ui.find("TitlePlaceholder").visible, "true");
            chk("preview default", ui.find("PreviewTime").text, "09:00");
            ui.focusText("TitleInput");
            ui.typeText(NEW_TITLE);
        }
        if (frames === 62) {
            chk("typed title", ui.find("TitleInput").text, NEW_TITLE);
            chk("placeholder hidden", ui.find("TitlePlaceholder").visible, "false");
            chk("snapTo hour 16", ui.snapTo("HourWheel", 16, 0), "true");
            chk("snapTo minute 30", ui.snapTo("MinuteWheel", 2, 0), "true");
        }
        if (frames === 64) {
            chk("hour snapIndex", ui.find("HourWheel").snapIndex, 16);
            chk("minute snapIndex", ui.find("MinuteWheel").snapIndex, 2);
            chk("preview 16:30", ui.find("PreviewTime").text, "16:30");
        }
        if (frames === 66) chk("tap save", ui.tap("BtnSave"), "true");
        if (frames === 90) {  // slideDown settled, deferred render() done
            chk("back on month", ui.currentFrame().name, "Month");
            chk("saved day dot", cellOf(EMPTY_DAY).find("Dot").visible, "true");
            chk("dots after save", dotCount(), PRESETS.length + 1);
            chk("saved agenda rows", agendaRows().length, 1);
            chk("saved agenda time", agendaRows()[0].find("EvTime").text, "16:30");
            chk("saved agenda title", agendaRows()[0].find("EvTitle").text, NEW_TITLE);
            chk("agenda count label", ui.find("AgendaCount").text, "1 项日程");
            chk("persisted", (localStorage.getItem(STORE_KEY) || "")
                .includes(NEW_TITLE), "true");
            let stored = null;
            try { stored = JSON.parse(localStorage.getItem(STORE_KEY)); } catch (e) {}
            chk("persisted count", stored && stored.events.length,
                PRESETS.length + 1);
        }
        if (frames === 92) {
            // scribble the agenda so the re-entry checks can't pass vacuously,
            // then leave and re-enter the day: render() must restore from state
            agendaRows()[0].find("EvTitle").text = "×";
            agendaRows()[0].find("EvTime").text = "××:××";
            ui.tap(cellOf(EVT_DAY));
        }
        if (frames === 96) chk("re-entry detour", agendaRows()[0]
            .find("EvTitle").text, "牙医");
        if (frames === 98) ui.tap(cellOf(EMPTY_DAY));
        if (frames === 102) {
            chk("restored title", agendaRows()[0].find("EvTitle").text, NEW_TITLE);
            chk("restored time", agendaRows()[0].find("EvTime").text, "16:30");
            chk("restored dot", cellOf(EMPTY_DAY).find("Dot").visible, "true");
        }
        // frame 110: _nav shot = Month with the saved event's day selected
        if (frames === 122) {
            const fails = checks.filter(([, got, want]) => got !== want);
            for (const [what, got, want] of checks)
                console.log(`bench check ${what}: got=${got} want=${want}`);
            console.log(fails.length ? "BENCH: FAIL" : "BENCH: PASS");
        }
    });
}
