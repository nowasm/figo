// _value_regress — benchmark app for G3 control semantics:
// ui.bindSlider (gesture -> value, engine-side knob/fill placement, step
// snapping, clamping, committed semantics, readonly, axis routing vs scroll),
// ui.setValue, ui.autoStates (hover/press -> automatic variant switching that
// must NOT eat the click), and ui.setVariant {duration} dissolve fade.
//
// Layout (420x900 viewport, scale 1):
//   Panel          : abs (24,120) 372x400, vertical scroll, maxScrollY = 200
//   Volume Track   : abs x 60..360, y 180..204 (center y 192), 0..100 step 5
//   Progress Track : abs x 60..360, y 260..272, readonly, 0..1
//   Demo Button    : abs (60,700) 120x44, center (120,722); the variant's
//                    "StateLabel" text spells the current state
// Assertions run in the SAME tick as their synthetic pointer calls wherever
// pointer state matters (the backend feeds the real mouse every frame).

ui.selectFrame(APP.entryFrame || "Main");

// ---- state (re-entrant on hot reload) ----
let changes = [];      // [value, committed] from Volume Track onChange
let trackClicks = 0;   // slider gestures must not click through
let btnClicks = 0;     // ...but autoStates must not eat real clicks

ui.bindSlider("Volume Track", {
    min: 0, max: 100, step: 5, value: 30,
    knob: "Volume Knob", fill: "Volume Fill",
    onChange: (v, committed) => changes.push([v, committed]),
});
ui.bindSlider("Progress Track", { min: 0, max: 1, value: 0.25,
                                  fill: "Progress Fill", readonly: true });
ui.autoStates("Demo Button");  // State=Hover/Pressed/Default convention

ui.onClick("Volume Track", () => trackClicks++);
ui.onClick("Demo Button", () => btnClicks++);

console.log("value_regress ready");

// ---- unattended tour (figoplay --selfdrive) ----
if (globalThis.SELFDRIVE) {
    const checks = [];
    const near = (v, want, tol) => Math.abs(v - want) <= tol;
    const check = (what, got, want) => checks.push([what, String(got), String(want)]);
    const fillW = () => ui.find("Volume Fill").width;
    const progW = () => ui.find("Progress Fill").width;
    const label = () => ui.find("Demo Button").find("StateLabel").text;
    const last = () => changes[changes.length - 1] || [];

    let frames = 0;  // count frames, not dt: first dt includes file load
    ui.onUpdate(() => {
        frames++;

        // bindSlider initial placement: value 30/100 -> 30% of 300 = 90;
        // progress 0.25 -> 75.
        if (frames === 10) {
            check("init fill", near(fillW(), 90, 1), "true");
            check("init progress", near(progW(), 75, 1), "true");
        }

        // Horizontal drag 110 -> 210 px: final lx = 150 -> value 50. The
        // move stream fires (v,false); the release commits (v,true).
        if (frames === 15) {
            ui.pointerDown(110, 192);
            ui.pointerMove(150, 192);
            ui.pointerMove(210, 192);
            ui.pointerUp(210, 192);
            check("drag value", `${last()[0]},${last()[1]}`, "50,true");
            check("drag live change",
                  changes.some(([v, c]) => v === 50 && c === false), "true");
            check("drag stepped", changes.every(([v]) => v % 5 === 0), "true");
            check("drag fill", near(fillW(), 150, 1), "true");
            check("drag no click", trackClicks, "0");
            check("drag no scroll", ui.find("Panel").scrollY, "0");
        }

        // Clamp at max: dragging past the right edge pins the value at 100.
        if (frames === 20) {
            ui.pointerDown(300, 192);
            ui.pointerMove(400, 192);
            ui.pointerMove(500, 192);
            ui.pointerUp(500, 192);
            check("clamp max", `${last()[0]},${last()[1]}`, "100,true");
            check("clamp fill", near(fillW(), 300, 1), "true");
        }

        // setValue: snaps (62 -> 60), places fill, fires no onChange.
        if (frames === 25) {
            const before = changes.length;
            check("setValue ok", ui.setValue("Volume Track", 62), "true");
            check("setValue snapped fill", near(fillW(), 180, 1), "true");
            check("setValue silent", changes.length - before, "0");
            ui.setValue("Volume Track", -20);  // clamp at min
            check("setValue clamp min", near(fillW(), 0, 1), "true");
        }

        // Tap-to-jump: a press+release on the track jumps and commits.
        if (frames === 30) {
            ui.pointerDown(240, 192);
            ui.pointerUp(240, 192);
            check("tap value", `${last()[0]},${last()[1]}`, "60,true");
            check("tap fill", near(fillW(), 180, 1), "true");
            check("tap no click", trackClicks, "0");
        }

        // Readonly progress bar: gestures pass through, setValue still works.
        if (frames === 35) {
            ui.pointerDown(100, 266);
            ui.pointerMove(160, 266);
            ui.pointerMove(230, 266);
            ui.pointerUp(230, 266);
            check("readonly ignores drag", near(progW(), 75, 1), "true");
            check("readonly setValue", ui.setValue("Progress Track", 0.5), "true");
            check("readonly fill", near(progW(), 150, 1), "true");
        }

        // Axis routing: a VERTICAL drag starting on the horizontal slider
        // belongs to the scroll container, not the slider.
        if (frames === 45) {
            const before = changes.length;
            ui.pointerDown(210, 192);
            ui.pointerMove(210, 140);
            ui.pointerMove(210, 90);
            ui.pointerUp(210, 90);
            check("cross-axis scrolls", ui.find("Panel").scrollY > 0, "true");
            check("cross-axis keeps value", near(fillW(), 180, 1), "true");
            check("cross-axis no change", changes.length - before, "0");
            ui.find("Panel").scrollY = 0;  // restore for later coordinates
        }

        // autoStates: hover -> press -> release-over -> leave, verified via
        // the variant's label text, all in the tick of the synthetic event.
        if (frames === 55) {
            ui.pointerMove(120, 722);
            check("auto hover", label(), "hover");
        }
        if (frames === 58) {
            ui.pointerDown(120, 722);
            check("auto pressed", label(), "pressed");
        }
        if (frames === 61) {
            ui.pointerUp(120, 722);
            check("auto release hover", label(), "hover");
            check("auto kept click", btnClicks, "1");  // swap didn't eat it
        }
        if (frames === 64) {
            ui.pointerMove(10, 880);
            check("auto leave", label(), "default");
        }

        // Dissolve: a slow setVariant fade must pass through a mid opacity
        // and land back on the authored value. Uses the "Fade Button"
        // instance, which has NO autoStates registration — the real OS
        // cursor hovering the window must not be able to restart its fade.
        if (frames === 75) {
            ui.setVariant("Fade Button", "State", "Pressed");  // instant
            check("instant label",
                  ui.find("Fade Button").find("StateLabel").text, "pressed");
            check("instant opacity", ui.find("Fade Button").opacity, "1");
            ui.setVariant("Fade Button", "State", "Default", { duration: 0.5 });
            check("dissolve label",
                  ui.find("Fade Button").find("StateLabel").text, "default");
        }
        if (frames === 85) {  // ~0.17s in: mid-fade
            const op = ui.find("Fade Button").opacity;
            check("dissolve mid opacity", op > 0 && op < 1, "true");
        }
        if (frames === 112) {  // ~0.6s in: settled back to authored
            check("dissolve done", near(ui.find("Fade Button").opacity, 1, 0.01),
                  "true");
        }

        if (frames === 120) {
            const fails = checks.filter(([, got, want]) => got !== want);
            for (const [what, got, want] of checks)
                console.log(`bench check ${what}: got=${got} want=${want}`);
            console.log(fails.length ? "BENCH: FAIL" : "BENCH: PASS");
        }
    });
}
