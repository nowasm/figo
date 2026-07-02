// music — benchmark app for G3 bindSlider (two live instances: seek +
// volume) and G6 playSound, plus a G5 onSwipe re-check on the cover.
//
// Model: 4 fictional tracks, simulated playback (1 wall second = 1 track
// second via setInterval), seek by dragging the progress slider, volume by
// dragging the volume slider, prev/play/next transport, cover swipe and
// playlist-row tap both switch tracks. Track switch plays a short synthesized
// blip (sounds/switch.wav) at the current volume. State lives in memory.
//
// Engine notes exercised here:
// - bindSlider is registered by NAME; the progress slider is RE-bound on
//   every track switch (new max = the track's duration). Re-binding must
//   replace the old registration, not stack.
// - ui.setValue drives the fill/knob every playback tick and never fires
//   onChange, so the "no crosstalk" bench count stays honest.
// - Two sliders coexist; gestures route by which track the pointer lands on.

ui.selectFrame(APP.entryFrame || "Music");

const TRACKS = [
    { title: "午夜快线", artist: "落日车队", dur: 227 },
    { title: "雾中信号", artist: "北岛磁带", dur: 252 },
    { title: "环形公路", artist: "青苔俱乐部", dur: 185 },
    { title: "慢速流星", artist: "银河巴士", dur: 278 },
];

// ---- state (in-memory; hot reload restarts the session, which is fine) ----
let cur = 0;
let pos = 0;            // seconds into the current track
let playing = false;
let volume = 70;        // 0..100
let seekEvents = [];    // [value, committed] from the progress slider
let volEvents = [];     // [value, committed] from the volume slider

const mmss = (s) => `${Math.floor(s / 60)}:${String(Math.floor(s % 60)).padStart(2, "0")}`;

// Progress slider: re-bound per track so max always equals the duration.
function bindProgress() {
    ui.bindSlider("Progress Track", {
        min: 0, max: TRACKS[cur].dur, step: 1, value: pos,
        knob: "Progress Knob", fill: "Progress Fill",
        onChange: (v, committed) => {
            pos = v;
            ui.setText("TimeCur", mmss(pos));
            seekEvents.push([v, committed]);
        },
    });
}

// Volume slider: bound once for the app's lifetime.
ui.bindSlider("Volume Track", {
    min: 0, max: 100, step: 1, value: volume,
    knob: "Volume Knob", fill: "Volume Fill",
    onChange: (v, committed) => {
        volume = v;
        volEvents.push([v, committed]);
    },
});

function applyRowState(row, i) {
    const on = i === cur;
    row.find("RowOn").visible = on;
    row.find("RowTitleOn").visible = on;
    row.find("RowTitle").visible = !on;
}

function renderList() {
    ui.bindList("TrackList", TRACKS.length, (row, i) => {
        const t = TRACKS[i];
        row.find("RowTitle").text = t.title;
        row.find("RowTitleOn").text = t.title;
        row.find("RowArtist").text = t.artist;
        row.find("RowTime").text = mmss(t.dur);
        applyRowState(row, i);
    });
}

function renderTrack() {
    const t = TRACKS[cur];
    for (let i = 0; i < TRACKS.length; i++)
        ui.setVisible(`Cover${i + 1}`, i === cur);
    ui.setText("TrackTitle", t.title);
    ui.setText("TrackArtist", t.artist);
    ui.setText("TimeCur", mmss(pos));
    ui.setText("TimeTotal", mmss(t.dur));
    ui.setText("PlayLabel", playing ? "暂停" : "播放");
    bindProgress();
    ui.findAll("TrackRow").forEach((row, i) => applyRowState(row, i));
}

function switchTrack(i) {
    cur = ((i % TRACKS.length) + TRACKS.length) % TRACKS.length;
    pos = 0;
    playing = true;
    // Purely informational: playSound returns false without an audio device
    // (web/silent CI) and the bench never depends on it.
    console.log("music blip played=" +
                ui.playSound("sounds/switch.wav", volume / 100));
    renderTrack();
}

function tick() {
    if (!playing) return;
    pos++;
    if (pos >= TRACKS[cur].dur) { switchTrack(cur + 1); return; }  // auto-next
    ui.setValue("Progress Track", pos);   // program-driven: no onChange
    ui.setText("TimeCur", mmss(pos));
}

// one interval for the app's lifetime; guard so hot reload doesn't stack them
if (globalThis.__musicTimer !== undefined) clearInterval(globalThis.__musicTimer);
globalThis.__musicTimer = setInterval(tick, 1000);

// ---- wire controls ----
ui.onClick("PlayBtn", () => {
    playing = !playing;
    ui.setText("PlayLabel", playing ? "暂停" : "播放");
});
ui.onClick("PrevBtn", () => switchTrack(cur - 1));
ui.onClick("NextBtn", () => switchTrack(cur + 1));
ui.onClick("TrackRow", (node) => {
    const i = node.index;
    if (i >= 0 && i < TRACKS.length && i !== cur) switchTrack(i);
});
ui.onSwipe("Cover", (node, dir) => {
    switchTrack(dir === "left" ? cur + 1 : cur - 1);
});

renderList();
renderTrack();
console.log("music ready");

// ---- unattended tour (figoplay --selfdrive) ----
// figoplay shoots frame 30 -> <prefix>_home.png (track 1 playing) and frame
// 110 -> <prefix>_nav.png (track 3 after the cover swipe), exits at 140.
// Geometry (absolute, gen_design.py): Progress Track x 40..380 center y 476,
// Volume Track center y 622, Cover center (210, 226).
if (globalThis.SELFDRIVE) {
    const checks = [];
    const chk = (what, got, want) => checks.push([what, String(got), String(want)]);
    const near = (v, want, tol) => Math.abs(v - want) <= tol;
    let frames = 0;  // count frames, not dt: first dt includes file load

    ui.onUpdate(() => {
        frames++;

        if (frames === 8) {
            chk("init title", ui.find("TrackTitle").text, "午夜快线");
            chk("init total", ui.find("TimeTotal").text, "3:47");
            chk("init cur", ui.find("TimeCur").text, "0:00");
            chk("init play label", ui.find("PlayLabel").text, "播放");
            chk("init row0 highlighted",
                ui.findAll("TrackRow")[0].find("RowTitleOn").visible, true);
        }

        if (frames === 12) ui.tap("PlayBtn");
        if (frames === 16) chk("playing label", ui.find("PlayLabel").text, "暂停");

        // frame 30: home shot shows track 1 playing

        // ~1.2s after Start at vsync: >= 1 playback tick elapsed.
        if (frames === 85) {
            const t = ui.find("TimeCur").text;
            chk("progress ticked", t === "0:00" ? "stuck at 0:00" : "ticked",
                "ticked");
        }

        // Drag-seek to the track midpoint: lx 170/340 -> 0.5 * 227 = 113.5,
        // step 1 snaps to 113 or 114. Whole gesture + assertions in ONE tick.
        if (frames === 88) {
            const before = seekEvents.length;
            ui.pointerDown(60, 476);
            ui.pointerMove(130, 476);
            ui.pointerMove(210, 476);
            ui.pointerUp(210, 476);
            const last = seekEvents[seekEvents.length - 1] || [];
            chk("seek committed", last[1], true);
            chk("seek value near mid", near(last[0], 113.5, 2), true);
            chk("seek live events", seekEvents.length - before >= 2, true);
            chk("seek time text", ui.find("TimeCur").text, mmss(last[0]));
        }

        // Volume drag to lx 102/340 -> 30. The progress slider must see NO
        // events (multi-instance isolation).
        if (frames === 92) {
            const pBefore = seekEvents.length;
            ui.pointerDown(300, 622);
            ui.pointerMove(220, 622);
            ui.pointerMove(142, 622);
            ui.pointerUp(142, 622);
            const last = volEvents[volEvents.length - 1] || [];
            chk("volume committed", `${last[0]},${last[1]}`, "30,true");
            chk("volume state", volume, 30);
            chk("volume no crosstalk", seekEvents.length - pBefore, 0);
        }

        if (frames === 96) ui.tap("NextBtn");
        if (frames === 100) {
            chk("next title", ui.find("TrackTitle").text, "雾中信号");
            chk("next total", ui.find("TimeTotal").text, "4:12");
            chk("next pos reset", pos <= 1, true);  // <=1: a tick may land
            const rows = ui.findAll("TrackRow");
            chk("next row1 highlighted", rows[1].find("RowTitleOn").visible, true);
            chk("next row0 cleared", rows[0].find("RowTitleOn").visible, false);
        }

        // Swipe the cover left -> next track (one tick, down/moves/up).
        if (frames === 104) {
            ui.pointerDown(300, 226);
            ui.pointerMove(250, 227);
            ui.pointerMove(180, 228);
            ui.pointerUp(180, 228);
        }
        if (frames === 107) {
            chk("swipe left title", ui.find("TrackTitle").text, "环形公路");
            chk("swipe left row2 highlighted",
                ui.findAll("TrackRow")[2].find("RowTitleOn").visible, true);
        }

        // frame 110: nav shot shows track 3 after the swipe

        if (frames === 112) {  // swipe right -> back to track 2
            ui.pointerDown(140, 226);
            ui.pointerMove(200, 227);
            ui.pointerMove(270, 228);
            ui.pointerUp(270, 228);
        }
        if (frames === 115)
            chk("swipe right title", ui.find("TrackTitle").text, "雾中信号");

        if (frames === 118) ui.tap(ui.findAll("TrackRow")[3]);
        if (frames === 121) {
            chk("row tap title", ui.find("TrackTitle").text, "慢速流星");
            chk("row tap total", ui.find("TimeTotal").text, "4:38");
            chk("row tap row3 highlighted",
                ui.findAll("TrackRow")[3].find("RowTitleOn").visible, true);
        }

        if (frames === 125) {
            const fails = checks.filter(([, got, want]) => got !== want);
            for (const [what, got, want] of checks)
                console.log(`bench check ${what}: got=${got} want=${want}`);
            console.log(fails.length ? "BENCH: FAIL" : "BENCH: PASS");
        }
    });
}
