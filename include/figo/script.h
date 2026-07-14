#pragma once
// figo — JS scripting host (QuickJS).
//
// Turns an app into <design.fig> + <logic.js>: the script gets a global `ui`
// object bound to a FigmaUI plus `console.log`. The host app only loads the
// two files and runs the frame loop.
//
// JS API (all names resolve within the current frame first, then document):
//   ui.onClick(name, fn(node, x, y))   ui.onHover(name, fn(node, entered, x, y))
//   ui.onLongPress(name, fn(node, x, y))  // pointer held >= 0.5s without
//     moving; fires once per press and consumes the release (no click). x/y
//     are viewport px (as in onClick/onHover; old 1-arg callbacks still work).
//   ui.onSwipe(name, fn(node, direction))  // "left" | "right"; horizontal
//     flick on release: |dx| >= 60 viewport px, |dx| > 2|dy|, < 0.5s. Bubbles
//     from the pressed node like click and consumes the release. A gesture
//     already eaten as HORIZONTAL drag-scroll doesn't swipe; a horizontal
//     flick over a vertical list still does (swipe-to-delete).
//   ui.onScroll(name, fn(node, x, y))  // the named scrolling frame's offset
//     changed (wheel/drag/fling/setScroll/scrollX writes); x/y = current
//     offset in frame px. Coalesced to one call per node per frame; a fling
//     that moves every frame fires every frame. No ancestor bubbling.
//   ui.onScrollEnd(name, fn(node, x, y, index))  // fires ONCE when the frame
//     comes fully to rest (wheel easing / fling / drag all over and the
//     offset stopped): a whole wheel train or fling = one call at the final
//     position. Instant sets (ui.setScroll, scrollX/Y writes) count as an end
//     of their own. index = the snapped child for a node.snapToChildren
//     container, -1 otherwise. No ancestor bubbling.
//   ui.snapTo(name, index, durationSec?) -> bool  // ease the scroll so child
//     `index` (clamped) aligns with the first child's start — the wheel-picker
//     "set initial value" call. durationSec omitted/<0 = stock easing feel,
//     0 = instant. Works on any scrolling frame, snapToChildren or not.
//   ui.scrollBy(x, y, dx, dy) -> bool  // wheel/trackpad scroll at a viewport
//     point (easing + ancestor bubbling + snap quantization — the real wheel
//     path); positive dy reveals content below.
//   ui.onUpdate(fn(dtSeconds))         ui.markDirty()
//   ui.navigateTo(name, transition?, durationSec?)   // "slideLeft" | "slideRight"
//   ui.navigateBack(durationSec?)                    // | "slideUp" | "slideDown"
//   ui.canGoBack() -> bool                           // | "dissolve" | "none"
//   ui.transitionProgress() -> number   // eased [0,1) mid-transition, 1 when
//                                       // idle — gate shots on >= 1
//   ui.selectFrame(name)               ui.frameNames() -> [string]
//   ui.currentFrame() -> node          ui.setResizeMode("reflow" | "scale")
//   ui.bindList(name, count, fn(itemNode, index))
//   ui.setText(name, s)  ui.setVisible(name, b)  ui.setOpacity(name, v)
//   ui.setVariant(name, property, value, {duration, animate}?)
//   ui.setScroll(name, x, y)
//     // duration (seconds) animates the switch. animate "dissolve" (default):
//     // the swapped-in variant subtree fades in from transparent, then
//     // returns to its authored opacity. animate "smart": Smart-Animate-style
//     // — children matched by name path between the old and new subtrees
//     // tween position/size/opacity/solid fill from the old state to the new
//     // authored state (cubic-out); unmatched new nodes fade in; falls back
//     // to dissolve when nothing pairs up
//   ui.bindSlider(name, {min, max, step?, value, knob?, fill?, axis?,
//                        readonly?, onChange(v, committed)?})
//     // slider/progress semantics on a designed track node (registered by
//     // NAME — survives bindList/setVariant rebuilds). The engine takes the
//     // pointer gesture on the track: a drag along the axis ("x" default,
//     // "y" vertical; left/top = min) maps linearly to [min,max] with step
//     // snapping, moves the knob / resizes the fill (both are child names of
//     // the track), and fires onChange(v,false) while dragging and (v,true)
//     // once on release. A tap jumps + commits. Same-axis drags beat
//     // scrolling; cross-axis drags fall through to the scroll container.
//     // readonly:true = progress bar (gestures pass through untouched).
//   ui.setValue(name, v)  // program-driven slider/progress value: clamps,
//     // snaps, places knob/fill; never fires onChange
//   ui.autoStates(name, {hover?, pressed?, base?}?)  // automatic variant
//     // switching for the named INSTANCE on hover/press/release (pressed
//     // wins over hover), values are "Prop=Value"; defaults State=Hover /
//     // State=Pressed / State=Default. Switches use a 0.12s dissolve and
//     // apply AFTER event dispatch, so they never eat the click. Variants
//     // missing from the set are skipped (soft no-op).
//   ui.setEditable(name, editable?)    ui.focusText(name)    ui.blur()
//   ui.setPassword(name, on?) -> bool  // TEXT node becomes a password field:
//     rendered (and caret/selection-measured) as one "•" per code point while
//     node.text stays plaintext. copy/cut return "" on it — passwords never
//     reach any clipboard. Also node.passwordMask (get/set).
//   ui.typeText(s)   // synthesized typing into the focused node, code point
//     by code point through the real textInput path (replaces the selection)
//   ui.editKey(k)    // synthesized edit keystroke: "left"|"right"|"home"|
//     "end"|"backspace"|"delete"|"enter" (inserts \n)|"selectAll"|"copy"|
//     "cut"|"paste". copy/cut return the copied string and, with paste, use
//     an IN-PROCESS simulated clipboard private to this ScriptHost — the
//     real backend chords (raylib: Ctrl/Cmd+C/X/V/A) use the OS clipboard;
//     the two never mix, so selfdrive tests don't depend on OS clipboard
//     state.
//   ---- Theme variables (design tokens) ----
//   Document-level named colors AND numbers, one value per mode
//   ("light"/"dark"/...). Paints bind to a color variable by name; numeric
//   node properties bind via ui.bindVar. Every mutation re-resolves the
//   bound literals, re-renders, and reflows when a layout-affecting numeric
//   binding changed — a whole app retints/re-spaces in one call. Color and
//   number variables are separate namespaces.
//   ui.setVariable(name, "#RRGGBB[AA]"|number, mode?) -> bool  // creates the
//     variable (and mode) on demand; mode omitted = ALL modes (rebrand);
//     string value = color variable, number value = number variable
//   ui.setVariables({name: "#hex"|number, ...}, mode?) -> bool  // batch form
//     — how a generated theme (seed color -> full palette) lands in one shot
//   ui.getVariable(name, mode?) -> "#hex"|number|null  // mode omitted =
//     active; colors win when both namespaces hold the name
//   ui.setThemeMode(mode) -> bool   // switch light/dark: re-resolves all
//     bound paints/props + re-renders; false when the mode doesn't exist
//   ui.themeMode() -> string        ui.themeModes() -> [string]
//   ui.bindFill(nameOrNode, varName) -> bool  // bind the node's first fill
//     (created solid if none) to a variable; "" clears the binding
//   ui.bindVar(nameOrNode, prop, varName) -> bool  // bind a numeric node
//     property to a number variable; prop: fontSize | cornerRadius |
//     strokeWeight | itemSpacing | paddingLeft/Right/Top/Bottom; "" clears
//     that property's binding (keeps the current literal)
//   ui.find(name) -> node|null         ui.findAll(name) -> [node]
//   ui.diagnostics(opts?) -> [{kind, node, id, message}]  // render problems
//     in the current frame a screenshot doesn't explain: kind =
//     "font-fallback" (requested family missing, silently substituted),
//     "text-overflow" (laid-out text exceeds its box) or "node-overflow"
//     (visible child clipped by a clipsContent parent; scroll axes exempt).
//     [] = clean. opts (all off by default) adds quality checks:
//     {textStress: 1.3} — pseudo-localization: fixed-size text that fits now
//       but overflows at ×1.3 content length -> "text-stress" (i18n guard;
//       auto-sizing text is exempt, it grows with content)
//     {touchTargets: true} — nodes with click/long-press handlers smaller
//       than 44px on a side -> "touch-target"
//     {states: true} — clickable instances whose component set has no
//       Hover/Pressed variant -> "state-coverage" (pair with ui.autoStates)
//   ui.playSound(path, volume?) -> bool  // fire-and-forget audio (wav/ogg/
//     mp3), volume 0..1 (default 1). Audio is a host capability injected via
//     ScriptHost::setAudioPlayer (figoplay: raylib, path resolved relative to
//     the app dir); without an injected player it is a quiet no-op returning
//     false — never an error — so web/silent environments keep working.
//     Returns whether the sound actually started (false on a bad path too).
//   ui.tap(nameOrNode) -> bool         // synthesized click at the node center
//   ui.longPress(nameOrNode) -> bool   // synthesized long press (one tick)
//   ui.pointerDown/pointerMove/pointerUp(x, y)  // raw pointer feed for
//     synthesized gestures; a multi-move gesture (drag/swipe) must complete
//     within ONE tick or the backend's real-mouse polling fights it
// By-name lookups (ui.find/setText/setVisible/...) search the current frame
// first, then the whole document. Structural mutations (ui.bindList /
// ui.setVariant) invalidate live node handles; don't call them from inside
// an onClick/onHover handler's node argument scope and re-find afterwards.
// Node objects (valid only while the underlying document node lives — don't
// hold them across bindList/setVariant/navigation):
//   node.name (get/set)   node.id   node.type        // "Text", "Frame", ...
//   node.text (get/set)   node.visible (get/set)     node.opacity (get/set)
//   node.scrollFixed (get/set)   node.childCount     node.child(i) -> node
//   node.passwordMask (get/set)   // see ui.setPassword
//   node.parent -> node|null           node.index    // position in parent
//   node.find(name) -> node|null       node.width / node.height  (read-only,
//                                      layout size in frame-local px)
//   node.scrollX / node.scrollY (get/set — write = instant, clamped; no-op on
//                                non-scrolling nodes; fires onScroll on change)
//   node.maxScrollX / node.maxScrollY  (read-only scroll range, frame px)
//   node.snapToChildren (get/set)  // runtime flag on a scrolling frame:
//     scrolls that come to a natural rest settle onto the nearest child
//     boundary (fling decays -> eases onto a child; slow drag release ->
//     same; a wheel notch quantizes to a child, one notch >= one item).
//     Boundary i = the offset aligning child i's main-axis start with the
//     first child's start (auto-layout padding/spacing included), clamped.
//   node.snapIndex  // read-only: nearest/snapped child index; -1 unless
//                   // snapToChildren is set
//   node.primarySizing = "hug"|"fixed"   node.primaryAlign = "min"|"center"|
//                                        "max"|"spaceBetween"   (auto-layout,
//                                        both also readable)
// Also available:
//   setTimeout(fn, ms) / setInterval(fn, ms) -> id, clearTimeout/clearInterval
//     (driven by update(dt) — they tick in app time, pausing with the host)
//   fetch(url, {method, headers, body}?) -> Promise<{status, ok, text(), json()}>
//     (the promise settles on the next update(dt); supported on Windows
//     (WinHTTP), Android (JNI HttpURLConnection — requires setAndroidJNI at
//     startup) and web (browser fetch, CORS applies). Windows/Android run on
//     a background thread with a 15s connect/read timeout; other platforms
//     reject with "not supported")
//   localStorage.getItem/setItem/removeItem/clear — string key/value store
//     persisted as JSON at the host-provided path (see setStoragePath);
//     without a path it works in memory only

#include <functional>
#include <memory>
#include <string>

namespace figo {

class FigmaUI;

// ---- Android platform bridge (generic JNI channel) ----
// fetch() needs JNI on Android today; future bridges (soft keyboard, share,
// clipboard, ...) ride the same channel — it stores the process JavaVM* plus
// the activity jobject (promoted to a JNI global reference), nothing
// fetch-specific. Call once at startup from android_main, before any script
// runs:
//     android_app* app = GetAndroidApp();
//     figo::setAndroidJNI(app->activity->vm, app->activity->clazz);
// void* keeps <jni.h> out of this header; all three functions exist on every
// platform (no-ops / null off Android) so callers need no #ifdef.
void setAndroidJNI(void* javaVM, void* activity);
void* androidJavaVM();    // injected JavaVM*, or null
void* androidActivity();  // activity jobject (global ref), or null

class ScriptHost {
public:
    explicit ScriptHost(FigmaUI& ui);
    ~ScriptHost();  // keep the host alive as long as the FigmaUI uses its handlers

    ScriptHost(const ScriptHost&) = delete;
    ScriptHost& operator=(const ScriptHost&) = delete;

    // Run a script file / source string. JS errors print to stderr -> false.
    bool runFile(const std::string& path);
    bool eval(const std::string& source, const std::string& filename = "<eval>");

    // Where localStorage persists (a small JSON file, created on first
    // setItem). Set it before runFile; figoplay uses "<script>.storage.json".
    void setStoragePath(const std::string& path);

    // Audio is a host capability (the core stays backend-agnostic): the host
    // injects a player and ui.playSound(path, volume) forwards to it. The
    // callback returns whether the sound actually started (load/decode
    // failure -> false). Without injection ui.playSound is a quiet no-op
    // returning false. figoplay injects a raylib LoadSound/PlaySound player
    // on desktop/Android (not on the web build — v1 skips web audio).
    void setAudioPlayer(std::function<bool(const std::string& path, float volume)> play);

    // Call once per host frame: fires ui.onUpdate handlers and drains the
    // JS job queue (promise reactions).
    void update(float dtSeconds);

    struct Impl;  // internal (public so the C binding functions can reach it)

private:
    std::unique_ptr<Impl> impl_;
};

}  // namespace figo
