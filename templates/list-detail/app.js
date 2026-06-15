// list-detail — a scrollable list that opens a detail screen on tap.
// Swap `data` for your own records and adjust the field names below.

const data = [
    { name: "Bitcoin", sub: "BTC", value: "$64,820", desc: "The original cryptocurrency." },
    { name: "Ethereum", sub: "ETH", value: "$3,410", desc: "Smart-contract platform." },
    { name: "Solana", sub: "SOL", value: "$152", desc: "High-throughput L1." },
    { name: "Cardano", sub: "ADA", value: "$0.45", desc: "Research-driven L1." },
    { name: "Polkadot", sub: "DOT", value: "$7.20", desc: "Cross-chain interoperability." },
    { name: "Chainlink", sub: "LINK", value: "$14.90", desc: "Decentralized oracles." },
];

ui.selectFrame(APP.entryFrame || "List");

ui.bindList("list", data.length, (item, i) => {
    item.find("row-name").text = data[i].name;
    item.find("row-sub").text = data[i].sub;
    item.find("row-value").text = data[i].value;
});

ui.onClick("Card", (node) => {
    if (node.parent.name !== "list") return;
    const d = data[node.index];
    if (!d) return;
    ui.setText("detail-title", d.name);
    ui.setText("detail-value", d.value);
    ui.setText("detail-desc", d.desc);
    ui.navigateTo("Detail", "slideLeft", 0.26);
});

ui.onClick("btn-back", () => ui.navigateBack(0.26));

console.log("list-detail ready —", data.length, "items");
