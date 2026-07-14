// theming — benchmark app: theme variables + Material You dynamic color.
// The design is the M3 component-library template (all fills bound to theme
// tokens). One switch = dark mode for the whole screen; two chips = live
// rebrand from a seed color (figoTheme -> HCT palette -> variable table).
// State persists in localStorage; the script is hot-reload re-entrant.

ui.selectFrame(APP.entryFrame || "Preview");

if (globalThis.SELFDRIVE) localStorage.removeItem("theming");

const SEEDS = { indigo: "#0b57d0", green: "#146c2e" };

let state = { dark: false, seed: "" };  // "" = the template's baseline palette
try {
    Object.assign(state, JSON.parse(localStorage.getItem("theming") || "{}"));
} catch (e) { /* defaults */ }

function save() { localStorage.setItem("theming", JSON.stringify(state)); }

function applyState() {
    if (state.seed) figoTheme.apply(state.seed);
    ui.setThemeMode(state.dark ? "dark" : "light");
    ui.setVariant("Switch Off", "State", state.dark ? "On" : "Off");
    ui.setVariant("Chip A", "State", state.seed === SEEDS.indigo ? "Selected" : "Default");
    ui.setVariant("Chip B", "State", state.seed === SEEDS.green ? "Selected" : "Default");
    // Re-label AFTER setVariant: variant swaps re-clone the master's children,
    // so per-instance text overrides must be re-applied (documented semantics).
    const chipA = ui.find("Chip A"), chipB = ui.find("Chip B");
    if (chipA) chipA.find("Label").text = "Indigo";
    if (chipB) chipB.find("Label").text = "Green";
}

ui.onClick("Switch Off", () => {
    state.dark = !state.dark;
    save();
    applyState();
});
ui.onClick("Chip A", () => { state.seed = SEEDS.indigo; save(); applyState(); });
ui.onClick("Chip B", () => { state.seed = SEEDS.green; save(); applyState(); });

// Buttons: automatic hover/press variants (the set ships State layers).
for (const b of ["Btn Filled", "Btn Tonal", "Btn Elevated", "Btn Outlined", "Btn Text"]) ui.autoStates(b);

// Slider: engine-managed gesture on the component's Fill/Knob children.
ui.bindSlider("Preview Slider", { min: 0, max: 100, value: 50 });

applyState();
console.log("theming ready");

// ---- unattended tour (figoplay --selfdrive) ----
// shots: frame 30 -> <prefix>_home.png (baseline light), frame 110 ->
// <prefix>_nav.png (dark + green rebrand). Host exits at frame 140.
if (globalThis.SELFDRIVE) {
    const seq = [
        [34, "Switch Off"],  // light -> dark
        [54, "Chip B"],      // rebrand to green seed
        [74, "Switch Off"],  // dark -> light
        [84, "Switch Off"],  // light -> dark again (idempotent toggle)
    ];
    let frames = 0;  // count frames, not dt: first dt includes file load
    const checks = [];
    ui.onUpdate(() => {
        frames++;
        for (const [at, name] of seq) {
            if (frames === at) console.log(`selfdrive tap ${name} ->`, ui.tap(name));
        }
        if (frames === 30) {
            checks.push(["initial mode", ui.themeMode(), "light"]);
            checks.push(["baseline primary", ui.getVariable("primary"), "#65558f"]);
        }
        if (frames === 44) checks.push(["dark on", ui.themeMode(), "dark"]);
        if (frames === 64) {
            const want = figoTheme.fromSeed(SEEDS.green).dark["primary"];
            checks.push(["rebrand primary", ui.getVariable("primary"), want]);
            checks.push(["chip selected", String(!!ui.find("Chip B")), "true"]);
        }
        if (frames === 80) checks.push(["back to light", ui.themeMode(), "light"]);
        if (frames === 94) {
            checks.push(["dark again", ui.themeMode(), "dark"]);
            const raw = localStorage.getItem("theming") || "";
            checks.push(["persisted", raw.includes('"dark":true') ? "yes" : "no", "yes"]);
        }
        if (frames === 100) {
            const fails = checks.filter(([, got, want]) => got !== want);
            for (const [what, got, want] of checks)
                console.log(`bench check ${what}: got=${got} want=${want}`);
            console.log(fails.length ? "BENCH: FAIL" : "BENCH: PASS");
        }
    });
}
