// form — editable text fields + a submit button. Tap a field to edit it
// (figmalib makes the TEXT node editable); submit reads the values back.

ui.selectFrame(APP.entryFrame || "Form");

// Make the inputs editable and focus one when its field row is tapped.
for (const [field, input] of [["field-name", "input-name"], ["field-email", "input-email"]]) {
    ui.setEditable(input);
    ui.onClick(field, () => ui.focusText(input));
}

ui.onClick("btn-submit", () => {
    const name = (ui.find("input-name").text || "").trim();
    const email = (ui.find("input-email").text || "").trim();
    if (!name || !email) {
        ui.setText("msg", "Please fill in both fields.");
        return;
    }
    ui.blur();
    ui.setText("msg", `Welcome, ${name}!`);
    console.log("submit:", { name, email });
});

ui.onHover("btn-submit", (node, entered) => ui.setOpacity(node, entered ? 0.88 : -1.0));

console.log("form ready");
