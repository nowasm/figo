// _token_regress — benchmark app for D1 numeric design tokens:
// number variables in the document variable table (per-mode values), node
// property bindings (varBindings: itemSpacing/padding*/fontSize), mode
// switching re-spacing hug stacks, live token edits (ui.setVariable with a
// number), runtime binding via ui.bindVar, and ui.getVariable across both
// namespaces (colors win on name clash; separate namespaces here).
//
// Layout (420x900, scale 1; heights are hug so JS can read them):
//   Stack  : itemSpacing+paddingTop/Bottom bound to "space" (compact 8/cozy 24)
//            hug height = 2*pad + 2*50 + spacing  → 124 compact / 172 cozy
//   Stack2 : no design-time bindings (authored spacing 8, height 124);
//            app.js binds itemSpacing → "space" at runtime
//   Label  : auto-height TEXT, fontSize bound to "text-md" (16/24) — the
//            wrapped height must GROW on cozy (exact value is font-dependent)

ui.setResizeMode("reflow");
ui.selectFrame(APP.entryFrame || "Main");

const checks = [];
function check(what, got, want) {
    checks.push([what, String(got), String(want)]);
}

if (globalThis.SELFDRIVE === undefined) globalThis.SELFDRIVE = true;

if (SELFDRIVE) {
    let frames = 0;
    let labelCompactH = 0;
    ui.onUpdate(() => {
        frames++;

        if (frames === 10) {
            // Variable reads across namespaces.
            check("number read", ui.getVariable("space"), "8");
            check("color read", ui.getVariable("accent"), "#ff0000");
            check("mode read", ui.getVariable("space", "cozy"), "24");
            check("missing read", ui.getVariable("nope"), "null");

            // Design-time bindings resolved at load (compact).
            check("stack compact", ui.find("Stack").height, "124");
            check("stack2 authored", ui.find("Stack2").height, "124");
            labelCompactH = ui.find("Label").height;
            check("label wrapped", labelCompactH > 0, "true");
        }

        if (frames === 20) {
            // Mode switch re-resolves bound props and reflows.
            check("mode switch", ui.setThemeMode("cozy"), "true");
            check("stack cozy", ui.find("Stack").height, "172");
            check("accent cozy", ui.getVariable("accent"), "#00ff00");
            check("label grew", ui.find("Label").height > labelCompactH, "true");
            // Unbound stack must not move on a mode switch.
            check("stack2 inert", ui.find("Stack2").height, "124");
        }

        if (frames === 30) {
            // Live token edit in the active mode only.
            check("live edit", ui.setVariable("space", 12, "cozy"), "true");
            check("stack live", ui.find("Stack").height, "136");  // 3*12 + 100

            // Runtime binding picks up the current value immediately.
            check("bindVar", ui.bindVar("Stack2", "itemSpacing", "space"), "true");
            check("stack2 bound", ui.find("Stack2").height, "128");  // 8+50+12+50+8

            // Validation: unknown prop / unknown variable / unbind.
            check("bad prop", ui.bindVar("Stack2", "rotation", "space"), "false");
            check("bad var", ui.bindVar("Stack2", "paddingTop", "nope"), "false");
            check("unbind", ui.bindVar("Stack2", "itemSpacing", ""), "true");
            ui.setVariable("space", 20, "cozy");  // unbound → Stack2 keeps 128
            check("unbound inert", ui.find("Stack2").height, "128");
            check("stack tracks", ui.find("Stack").height, "160");  // 3*20 + 100
        }

        if (frames === 40) {
            const fails = checks.filter(([, got, want]) => got !== want);
            for (const [what, got, want] of checks)
                console.log(`bench check ${what}: got=${got} want=${want}`);
            console.log(fails.length ? "BENCH: FAIL" : "BENCH: PASS");
        }
    });
}

console.log("token_regress ready");
