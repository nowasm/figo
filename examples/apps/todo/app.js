// todo — benchmark app for the G5 gesture surface in a real app shape:
// tap row = toggle done, swipe left = delete, long press = pin to top,
// Add button pulls the next candidate task (text input lands with G1).
//
// Engine notes:
// - Structural changes (delete/pin/add) rebind the list; bindList is illegal
//   inside an event handler (it frees nodes mid-dispatch), so handlers
//   mutate data synchronously and defer render() via setTimeout(0).
// - Toggle is non-structural: mutate the row in place (habits pattern).
// - Done styling: dual TEXT (fg/muted) setVisible swap + row opacity, since
//   the renderer doesn't draw textDecoration.

ui.selectFrame(APP.entryFrame || "Todo");

const INITIAL = [
    "回复房东的续租邮件",
    "给阳台的绿萝浇水",
    "预约周五的牙医复诊",
    "整理上周的报销发票",
    "下载周末骑行路线离线地图",
];
const CANDIDATES = ["给妈妈回个电话", "取干洗店的外套", "续费健身房会员"];
const STORE_KEY = "todo.state.v1";

// Bench runs must be idempotent: wipe persisted state before loading it.
if (globalThis.SELFDRIVE) localStorage.removeItem(STORE_KEY);

let state = { tasks: INITIAL.map(t => ({ t, d: false })), pool: CANDIDATES.slice() };
try {
    const saved = JSON.parse(localStorage.getItem(STORE_KEY) || "null");
    if (saved && Array.isArray(saved.tasks) && Array.isArray(saved.pool))
        state = saved;
} catch (e) { /* corrupt store -> start fresh */ }

function save() {
    localStorage.setItem(STORE_KEY, JSON.stringify(state));
}

// Row visuals from state. `row` is a live TaskRow clone; only touched during
// a bind pass or an in-place toggle (never held across binds).
function applyRow(row, i) {
    const { t, d } = state.tasks[i];
    row.find("TaskText").text = t;
    row.find("TaskTextDone").text = t;
    row.find("TaskText").visible = !d;
    row.find("TaskTextDone").visible = d;
    row.find("CheckOn").visible = d;
    row.find("CheckOff").visible = !d;
    row.opacity = d ? 0.6 : 1.0;
}

function applyHeader() {
    const left = state.tasks.filter(x => !x.d).length;
    ui.setText("RemainLabel", `还剩 ${left} 项`);
    ui.setText("NextLabel", state.pool.length ? state.pool[0] : "候选任务用完了");
    ui.setOpacity("AddBtn", state.pool.length ? 1.0 : 0.4);
}

// Full render: rebind the list (re-entrant — bindList reuses the stored
// template on hot reload and on every structural change).
function render() {
    ui.bindList("TaskList", state.tasks.length, (item, i) => applyRow(item, i));
    applyHeader();
}
render();

// ---- interactions ----
ui.onClick("TaskRow", (node) => {  // tap = toggle done (in place, no rebind)
    const i = node.index;
    if (i < 0 || i >= state.tasks.length) return;
    state.tasks[i].d = !state.tasks[i].d;
    save();
    applyRow(node, i);
    applyHeader();
});

ui.onSwipe("TaskRow", (node, dir) => {  // swipe left = delete
    if (dir !== "left") return;
    const i = node.index;  // read now: node dies with the rebind
    if (i < 0 || i >= state.tasks.length) return;
    setTimeout(() => {  // bindList must run outside event dispatch
        state.tasks.splice(i, 1);
        save();
        render();
    }, 0);
});

ui.onLongPress("TaskRow", (node) => {  // long press = pin to top
    const i = node.index;
    if (i <= 0 || i >= state.tasks.length) return;
    setTimeout(() => {
        state.tasks.unshift(state.tasks.splice(i, 1)[0]);
        save();
        render();
    }, 0);
});

ui.onClick("AddBtn", () => {  // pull the next candidate into the list
    if (!state.pool.length) return;
    setTimeout(() => {
        state.tasks.push({ t: state.pool.shift(), d: false });
        save();
        render();
    }, 0);
});

console.log("todo ready");

// ---- unattended tour (figoplay --selfdrive) ----
// figoplay shoots frame 30 -> <prefix>_home.png and frame 110 ->
// <prefix>_nav.png, exits at 140. Tour: toggle row1 -> add a candidate ->
// swipe-delete row0 -> long-press row2 to pin -> verify persistence.
// TaskList sits at y=252, rows 64 high with 10 spacing: row i's on-screen
// center is (210, 252 + i*74 + 32) while scroll is 0.
if (globalThis.SELFDRIVE) {
    const checks = [];
    const push = (what, got, want) => checks.push([what, String(got), String(want)]);
    const rows = () => ui.findAll("TaskRow");
    const rowText = (i) => rows()[i].find("TaskText").text;
    const rowDone = (i) => rows()[i].find("TaskTextDone").visible;
    let frames = 0;  // count frames, not dt: first dt includes file load

    ui.onUpdate(() => {
        frames++;

        if (frames === 40) console.log("selfdrive tap row1 ->", ui.tap(rows()[1]));
        if (frames === 46) {
            push("toggle: remaining", ui.find("RemainLabel").text, "还剩 4 项");
            push("toggle: row1 done text shown", rowDone(1), true);
            push("toggle: row1 fg text hidden", rows()[1].find("TaskText").visible, false);
            push("toggle: row1 check filled", rows()[1].find("CheckOn").visible, true);
        }

        if (frames === 52) console.log("selfdrive tap add ->", ui.tap("AddBtn"));
        if (frames === 58) {
            push("add: row count", rows().length, 6);
            push("add: new row text", rowText(5), CANDIDATES[0]);
            push("add: next candidate", ui.find("NextLabel").text, CANDIDATES[1]);
            push("add: remaining", ui.find("RemainLabel").text, "还剩 5 项");
        }

        if (frames === 64) {  // swipe row0 left: one tick, down/moves/up
            ui.pointerDown(340, 284);
            ui.pointerMove(300, 285);
            ui.pointerMove(240, 286);
            ui.pointerMove(160, 286);
            ui.pointerUp(160, 286);
        }
        if (frames === 72) {
            push("delete: row count", rows().length, 5);
            push("delete: new row0 text", rowText(0), INITIAL[1]);
            push("delete: row0 kept done state", rowDone(0), true);
            push("delete: remaining", ui.find("RemainLabel").text, "还剩 4 项");
        }

        if (frames === 80)
            console.log("selfdrive longpress row2 ->", ui.longPress(rows()[2]));
        if (frames === 88) {
            push("pin: row0 is pinned task", rowText(0), INITIAL[3]);
            push("pin: row0 not done", rowDone(0), false);
            push("pin: old row0 shifted down", rowText(1), INITIAL[1]);
            push("pin: row count unchanged", rows().length, 5);
            push("pin: remaining unchanged", ui.find("RemainLabel").text, "还剩 4 项");
        }

        if (frames === 96) {
            push("persist: store matches state",
                 localStorage.getItem(STORE_KEY), JSON.stringify(state));
            push("persist: order", state.tasks.map(x => x.t).join("|"),
                 [INITIAL[3], INITIAL[1], INITIAL[2], INITIAL[4], CANDIDATES[0]].join("|"));
        }

        if (frames === 120) {
            const fails = checks.filter(([, got, want]) => got !== want);
            for (const [what, got, want] of checks)
                console.log(`bench check ${what}: got=${got} want=${want}`);
            console.log(fails.length ? "BENCH: FAIL" : "BENCH: PASS");
        }
    });
}
