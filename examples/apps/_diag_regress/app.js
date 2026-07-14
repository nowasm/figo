// _diag_regress — benchmark app for the D9/D10 opt-in diagnostics:
// ui.diagnostics(opts) with textStress (pseudo-localization overflow
// prediction), touchTargets (interactive nodes < 44px) and states (clickable
// instances without hover/pressed variants). The zero-arg call must stay
// exactly as before (base kinds only) so "empty array = clean" holds.
//
// Layout (420x900): TightLabel 150px box (fits now, clips at x1.3),
// RoomyLabel 360px (survives stress), TinyBtn 32x28 + BigBtn 120x48 +
// StatelessBtn (instance, set has no hover/pressed) — all three clickable.

ui.selectFrame(APP.entryFrame || "Main");

const checks = [];
function check(what, got, want) {
    checks.push([what, String(got), String(want)]);
}

ui.onClick("TinyBtn", () => {});
ui.onClick("BigBtn", () => {});
ui.onClick("StatelessBtn", () => {});

if (globalThis.SELFDRIVE === undefined) globalThis.SELFDRIVE = true;

if (SELFDRIVE) {
    let frames = 0;
    ui.onUpdate(() => {
        frames++;

        if (frames === 10) {
            const kinds = (list) => list.map((d) => `${d.kind}:${d.node}`).sort();

            // Base call: unchanged contract (this design is clean).
            check("base clean", ui.diagnostics().length, "0");

            // Pseudo-localization: the tight box trips, the roomy one doesn't.
            const stress = ui.diagnostics({ textStress: 1.3 });
            check("stress hits tight",
                  kinds(stress).includes("text-stress:TightLabel"), "true");
            check("stress spares roomy",
                  kinds(stress).some((k) => k.endsWith("RoomyLabel")), "false");

            // Touch targets: only the sub-44px clickable is flagged.
            const touch = ui.diagnostics({ touchTargets: true });
            check("touch hits tiny",
                  kinds(touch).includes("touch-target:TinyBtn"), "true");
            check("touch spares big",
                  kinds(touch).some((k) => k.endsWith("BigBtn")), "false");

            // State coverage: the instance whose set lacks hover/pressed.
            const states = ui.diagnostics({ states: true });
            check("states hits plain",
                  kinds(states).includes("state-coverage:StatelessBtn"), "true");

            // Combined call returns all opt-in kinds at once.
            const all = ui.diagnostics({ textStress: 1.3, touchTargets: true, states: true });
            check("combined count", all.length >= 3, "true");
        }

        if (frames === 20) {
            const fails = checks.filter(([, got, want]) => got !== want);
            for (const [what, got, want] of checks)
                console.log(`bench check ${what}: got=${got} want=${want}`);
            console.log(fails.length ? "BENCH: FAIL" : "BENCH: PASS");
        }
    });
}

console.log("diag_regress ready");
