// Alarm listener on the LP-MSPM0G5187: methods, memory, CPU, M0+ vs M33 vs NPU, tests, results, path to a product.
// Every number comes from mspm0g5187/alarm/results/, data/results/*.csv or the build map; estimates say so.
// Five-slide summary of build.js (the full deck). Build: NODE_PATH=../../../tools/deck/node_modules node build-short.js [out.pptx]
const pptxgen = require("pptxgenjs");
const OUT = process.argv[2] || "alarm-listener-summary.pptx";
// Fox Red Labs brand: Paper background, Ink text, Fox Red the only accent, warm neutrals for data.
const INK = "161312", STONE = "6B625B", LINE = "D9D1C5", PAPER = "F4EFE6", MIST = "F7F3EC", ACC = "B42418",
  OK = "2E6B3F", AMBER = "8E1C13", RED = "8E1C13", NPU = "B42418", CPU = "4A423C", ISR = "8C827A";
const LOGO = require("path").join(__dirname, "../../../brand/kit/png/horizontal-red-2000.png");
const W = 13.333, M = 0.5, F = "Barlow";
const DISCLOSE = "Fox Red Labs is an independent project. Its author works at Texas Instruments. It is not affiliated with, sponsored by, or endorsed by Texas Instruments.";
const FOOT = "Fox Red Labs. Bench results 12 to 13 Oct 2026. LP-MSPM0G5187 Rev A, MSPM0 SDK 2.11, TI Arm Clang 5.1.1, TI NNC 2.1.2. Measured unless marked estimate.";
const pres = new pptxgen(); pres.layout = "LAYOUT_WIDE"; pres.title = "Alarm listener: summary";
const T = (t, o = {}) => ({ text: t, options: Object.assign({ fontFace: F, fontSize: 11, color: INK }, o) });
let s, n = 0;
const ONLY = +(process.env.ONLY || 0);   // ONLY=k builds slide k alone (for a Quick Look preview)
const NOOP = { addText() {}, addShape() {}, addTable() {}, addChart() {}, addImage() {} };
function frame(title, sub) {
  n++;
  if (ONLY && n !== ONLY) { s = NOOP; return; }
  s = pres.addSlide(); s.background = { color: PAPER };
  if (n === 1) s.addText(DISCLOSE, { x: M, y: 6.85, w: 12.3, h: 0.25, fontFace: F, fontSize: 8.5, color: STONE, margin: 0 });
  s.addImage({ path: LOGO, x: W - 0.5 - 1.4, y: 0.22, w: 1.4, h: 1.4 * 989 / 2000 });
  s.addText(title, { x: M, y: 0.25, w: 10.9, h: 0.6, fontFace: F, fontSize: 24, bold: true, color: INK, margin: 0 });
  if (sub) s.addText(sub, { x: M, y: 0.85, w: 10.9, h: 0.5, fontFace: F, fontSize: 13, color: STONE, margin: 0 });
  s.addText(FOOT, { x: M, y: 7.1, w: 11.5, h: 0.25, fontFace: F, fontSize: 8.5, color: STONE, margin: 0 });
  s.addText(String(n), { x: W - 0.9, y: 7.1, w: 0.4, h: 0.25, fontFace: F, fontSize: 9, color: STONE, margin: 0, align: "right" });
}
const sec = (t, x, y, w, col = INK) => s.addText(t, { x, y, w, h: 0.32, fontFace: F, fontSize: 14, bold: true, color: col, margin: 0 });
const box = (x, y, w, h, fill = MIST, line = LINE) => s.addShape(pres.shapes.ROUNDED_RECTANGLE, { x, y, w, h, fill: { color: fill }, line: { color: line, width: 1 }, rectRadius: 0.06 });
const rect = (x, y, w, h, col) => s.addShape(pres.shapes.RECTANGLE, { x, y, w, h, fill: { color: col }, line: { color: col, width: 0.5 } });
const txt = (runs, x, y, w, h, o = {}) => s.addText(runs, Object.assign({ x, y, w, h, margin: 0, valign: "top" }, o));
const hdr = cols => cols.map(c => T(c, { bold: true, color: PAPER, fill: { color: INK }, fontSize: 10.5 }));
const tbl = (rows, o) => s.addTable(rows, Object.assign({ x: M, fontFace: F, border: { type: "solid", pt: 0.75, color: LINE }, fill: { color: MIST }, valign: "middle", margin: 0.05 }, o));
const note = (t, x, y, w, h) => { s.addShape(pres.shapes.RECTANGLE, { x, y, w, h, fill: { color: "EAE3D8" }, line: { color: LINE } }); txt(t, x + 0.15, y + 0.08, w - 0.3, h - 0.16, { valign: "middle" }); };
const arrow = (x1, y1, x2, y2) => s.addShape(pres.shapes.LINE, { x: x1, y: y1, w: x2 - x1, h: y2 - y1, line: { color: STONE, width: 1.5, endArrowType: "triangle" } });
// Bars drawn as shapes so every viewer shows them. rows: [label, value, colour, shownText]
function hbars(x, y, w, rows, { max, log = false, labelW = 2.6, rowH = 0.5, unit = "" } = {}) {
  const bw = w - labelW - 0.9, f = v => log ? Math.max(0, Math.log10(v / log) / Math.log10(max / log)) : v / max;
  rows.forEach(([label, v, col, shown], i) => {
    const yy = y + i * rowH;
    txt([T(label, { fontSize: 10.5 })], x, yy, labelW - 0.1, rowH, { valign: "middle", align: "right" });
    rect(x + labelW, yy + rowH * 0.18, Math.max(bw * f(v), 0.03), rowH * 0.64, col);
    txt([T(shown || `${v}${unit}`, { fontSize: 10.5, bold: true })], x + labelW + Math.max(bw * f(v), 0.03) + 0.08, yy, 1.4, rowH, { valign: "middle" });
  });
}
function vbars(x, y, w, h, rows, { max, unit = "" } = {}) {
  const n = rows.length, cw = w / n, bw = cw * 0.6;
  rect(x, y + h, w, 0.015, STONE);
  rows.forEach(([label, v, col, shown], i) => {
    const bh = h * v / max, bx = x + i * cw + (cw - bw) / 2;
    if (bh > 0) rect(bx, y + h - bh, bw, bh, col);
    txt([T(shown || `${v}${unit}`, { fontSize: 13, bold: true, color: col })], bx - 0.3, y + h - bh - 0.38, bw + 0.6, 0.35, { align: "center", valign: "bottom" });
    txt([T(label, { fontSize: 10.5 })], x + i * cw, y + h + 0.05, cw, 0.5, { align: "center" });
  });
}
const yes = (t = "Yes") => T(t, { bold: true, color: OK, fontSize: 10 });
const nope = t => T(t, { bold: true, color: RED, fontSize: 10 });
const c = (t, o = {}) => T(t, Object.assign({ fontSize: 10 }, o));


// 1. The answer and how it works
frame("It learns your smoke alarm and tells you when it sounds",
  "An MSPM0G5187 with a TinyEngine NPU listens through its own microphone, tells smoke from CO, and ignores every other sound.");
const kpi = (x, big, small, col = INK) => { box(x, 1.5, 3.95, 1.45, MIST); txt([T(big, { fontSize: 32, bold: true, color: col, breakLine: true }), T(small, { fontSize: 12 })], x + 0.2, 1.6, 3.55, 1.3); };
kpi(M, "0 false alarms", "in 3.4 h of a real room overnight, and in 1.6 h of simulated house sound", OK);
kpi(M + 4.15, "97% caught", "162 of 167 simulated alarms with the board beside the alarm");
kpi(M + 8.3, "2% of the CPU", "for the neural net, once a second; 80 kB flash, 30 kB RAM in all", ACC);
sec("How it works", M, 3.2, 6);
const st = [["Hear", "microphone, 16k samples a second", "6B625B"], ["Describe", "13 sound numbers every 64 ms", CPU], ["Recognize", "neural net on the NPU, every second: other, smoke or CO", NPU], ["Check timing", "exact fire-code beep pattern", CPU], ["Is it mine?", "same tone and loudness as the alarm it learned", CPU], ["Tell you", "Home Assistant: smoke or CO", OK]];
st.forEach(([h, b, col], i) => {
  const x = M + i * 2.07;
  box(x, 3.6, 1.9, 1.35); rect(x, 3.6, 1.9, 0.12, col);
  txt([T(h, { bold: true, fontSize: 13, breakLine: true }), T(b, { fontSize: 10.5, color: STONE })], x + 0.12, 3.8, 1.68, 1.1);
  if (i < st.length - 1) arrow(x + 1.9, 4.28, x + 2.07, 4.28);
});
note([T("To set it up: ", { bold: true }), T("put it beside an ordinary smoke or CO alarm, press its button, then the alarm's test button. It learns that alarm. "),
  T("Status: ", { bold: true, color: AMBER }), T("proven with the standard alarm patterns from a speaker and over USB; a real smoke alarm's horn is the next test.")], M, 5.3, 12.3, 1.5);

// 2. Why it does not false alarm
frame("Three clues tell your alarm apart from every other sound", "Each clue rules out a different kind of false alarm. All three must agree.");
const tl = (y, label, beeps, col, desc) => {
  txt([T(label, { bold: true, fontSize: 11.5 })], M, y, 2.2, 0.45, { valign: "middle" });
  rect(2.8, y + 0.37, 7.44, 0.02, LINE);
  beeps.forEach(([a, b]) => rect(2.8 + a * 0.62, y + 0.05, Math.max((b - a) * 0.62, 0.035), 0.32, col));
  txt([T(desc, { fontSize: 10.5, color: STONE })], 10.45, y, 2.5, 0.45, { valign: "middle" });
};
const t3 = []; for (let k = 0; k < 3; k++) for (let i = 0; i < 3; i++) t3.push([k * 4 + i, k * 4 + i + 0.5]);
const t4 = []; for (let k = 0; k < 3; k++) for (let i = 0; i < 4; i++) { const a = k * 5.8 + i * 0.2; if (a < 12) t4.push([a, a + 0.1]); }
sec("The fire code fixes the beep pattern (NFPA 72)", M, 1.45, 8);
tl(1.85, "Smoke", t3, RED, "three beeps, 1.5 s quiet");
tl(2.4, "CO", t4, NPU, "four pulses, 5 s quiet");
const clue = (x, title, body, beats) => { box(x, 3.2, 3.95, 2.3); txt([T(title, { bold: true, fontSize: 15, color: ACC, breakLine: true }), T(body, { fontSize: 12, breakLine: true }), T("Stops: ", { bold: true, fontSize: 12 }), T(beats, { fontSize: 12 })], x + 0.2, 3.32, 3.55, 2.1, { paraSpaceAfter: 6 }); };
clue(M, "Pattern", "A neural net spots alarm-like sound; a timing check insists on the exact pattern.", "timers, microwaves, alarm clocks, phones, whistles.");
clue(M + 4.15, "Tone", "One test-button press teaches it the horn's pitch. Later alarms must match within 4%.", "other alarms and look-alike beeps.");
clue(M + 8.3, "Loudness", "It learns how loud the alarm is right beside it. Anything 10 dB quieter is not that alarm.", "the same pattern from a TV or a neighbour's house.");
note([T("Why both a net and rules: ", { bold: true }), T("alone, the net gave 7 false alarms in 1.6 hours of simulated house sound. With the timing check: 0. The live test also caught two CO look-alikes the simulation missed; the CO rule now needs two pulse groups 5.8 s apart, as real CO alarms repeat.")], M, 5.75, 12.3, 1.15);

// 3. Results
frame("Results: right on every test case on the board", "Simulation used 600 house scenes with sounds the model never heard. Board tests fed audio over USB and played it through the air.");
sec("False alarms, 1.6 h simulated", M, 1.45, 3.8);
vbars(M + 0.2, 2.15, 3.4, 2.0, [["Net alone", 7, RED], ["Net + timing", 0, OK]], { max: 7 });
sec("Alarms caught, by loudness over the room", M, 4.85, 4.5);
hbars(M, 5.2, 4.3, [["Beside the alarm", 97, OK, "97%"], ["40+ dB", 94, CPU, "94%"], ["0 to 10 dB", 64, CPU, "64%"]], { max: 100, labelW: 1.6, rowH: 0.48 });
tbl([hdr(["On the board", "Result", ""]),
  [c("Learn a smoke alarm (button, then the alarm)"), c("Learned 3,162 Hz; kept after a reboot"), yes()],
  [c("That alarm sounds"), c("ALARM smoke"), yes()],
  [c("CO pattern from the same horn"), c("ALARM co"), yes()],
  [c("18 decoys: timers, beepers, whistle, kettle, doorbell, phone, speech, music"), c("Nothing"), yes()],
  [c("Same alarm 22 to 31 dB quieter (a TV)"), c("Ignored: too quiet"), yes()],
  [c("An alarm at another pitch"), c("Ignored: tone differs"), yes()],
  [c("Through the air from a speaker: smoke pattern"), c("ALARM smoke"), yes()],
  [c("Through the air: 16 decoys"), c("14 quiet; 2 CO look-alikes alarmed, then fixed"), nope("Fixed")],
  [c("Real room overnight, 3.4 h"), c("0 alarms in 12,094 decisions"), yes()],
  [c("A real smoke alarm's horn", { bold: true }), c("Next test", { bold: true, color: AMBER }), c("")],
], { x: 5.3, y: 1.45, w: 7.5, colW: [3.9, 2.85, 0.75], rowH: 0.47 });

// 4. What it costs the chip
frame("What it costs the chip: the AI is the cheap part", "Measured on the board at 80 MHz, and from the linker map.");
sec("CPU, each second", M, 1.45, 5);
[["Audio filter (interrupt)", 43.0, ISR], ["Sound features", 11.0, CPU], ["Neural net on the NPU", 1.9, NPU], ["Idle", 44.1, "E5DED3"]].forEach(([lab, v, col], i) => {
  const y = 1.85 + i * 0.6;
  txt([T(lab, { fontSize: 10.5 })], M, y - 0.05, 4.8, 0.25);
  rect(M, y + 0.2, 3.0 * v / 50, 0.28, col);
  txt([T(`${v}%`, { bold: true, fontSize: 12 })], M + 3.0 * v / 50 + 0.1, y + 0.17, 1, 0.34, { valign: "middle" });
});
sec("Memory", M, 4.4, 5);
[["Flash", 80.6, 128], ["RAM", 30.4, 32]].forEach(([lab, u, t], i) => {
  const y = 4.8 + i * 0.6;
  txt([T(`${lab}: ${u} of ${t} kB`, { fontSize: 11 })], M, y - 0.05, 4.8, 0.25);
  rect(M, y + 0.2, 5.0, 0.28, MIST); rect(M, y + 0.2, 5.0 * u / t, 0.28, i ? ACC : CPU);
});
txt([T("24.6 kB of the flash is self-test data a product drops; RAM is the tight one.", { fontSize: 10, color: STONE })], M, 6.05, 5.5, 0.5);
sec("Same kind of model on other chips (8k-weight model, ms per answer, measured)", 6.4, 1.45, 6.6);
hbars(6.4, 1.85, 6.4, [["M0+ 80 MHz (this chip's CPU)", 35.3, CPU], ["M33 160 MHz", 7.38, "8C827A"], ["M33 200 MHz", 5.06, "8C827A"], ["This chip's NPU, 80 MHz", 2.12, NPU]], { max: 40, labelW: 2.6, rowH: 0.5, unit: " ms" });
note([T("What it means: ", { bold: true }), T("the NPU makes this M0+ about 17 times faster on AI, beating an M33 at twice the clock. The biggest CPU job is not the AI but slowing the audio down; the audio chip can sample at 16 kHz itself, which should free most of that 43% (next step).")], 6.4, 4.2, 6.4, 1.6);

// 5. Toward a product
frame("Toward a product: one listener per alarm, joined over Zigbee", "Recommendation for a stick-on, battery-capable listener in a home that already has a smart-home hub.");
sec("Which radio", M, 1.45, 6);
tbl([hdr(["Radio", "Fit", "Why"]),
  [c("Zigbee", { bold: true }), c("Recommended", { bold: true, color: OK }), c("Low power (battery), mesh reaches every floor, standard smoke and CO sensor types every hub shows. Our CC2340R5 already joined Home Assistant over Zigbee here.")],
  [c("Thread / Matter", { bold: true }), c("Good too"), c("Low power, works with Apple and Google homes; needs a border router (an Apple TV or hub).")],
  [c("Sub-GHz", { bold: true }), c("Niche"), c("Best range through floors, but no standard home hub; we would build the receiver. Its wires on this board share the microphone's pins.")],
  [c("Wi-Fi", { bold: true }), c("Plug-in only"), c("No hub needed, but too power-hungry for a battery listener.")],
], { y: 1.85, w: 6.1, colW: [1.25, 1.2, 3.65], rowH: 0.72 });
txt([T("Used so far: no radio. The board talks over USB to a Mac, which posts to Home Assistant.", { fontSize: 10.5, color: STONE })], M, 5.6, 6.1, 0.4);
sec("How many in a house", 6.85, 1.45, 6);
txt([T("It listens to an alarm, not the room, so the count follows the alarms, not the square footage.", { fontSize: 12, bold: true, breakLine: true }),
  T("Alarms linked so all sound together (required in new homes): ", { fontSize: 11.5, bold: true }), T("one listener beside any one alarm hears a fire anywhere. One per floor adds backup.", { fontSize: 11.5, breakLine: true }),
  T("Older, unlinked alarms: ", { fontSize: 11.5, bold: true }), T("one per alarm. The code calls for one in each bedroom, outside each sleeping area and on every level, so a two-floor, three-bedroom house has about five or six.", { fontSize: 11.5, breakLine: true }),
  T("Range: ", { fontSize: 11.5, bold: true }), T("by design it reacts only within about 10 dB of the alarm it learned, roughly three times the distance it learned from. Hearing range has not been measured yet.", { fontSize: 11.5 })],
  6.85, 1.85, 6.0, 3.6, { paraSpaceAfter: 8 });
note([T("Next steps: ", { bold: true, color: ACC }), T("1. Learn and test a real smoke alarm.  2. Zigbee link: the board tells the CC2340R5, which reports to Home Assistant as a smoke or CO sensor.  3. Sample at 16 kHz in the audio chip and re-measure CPU and power.  4. Record 10+ real alarm brands, including low-tone and combination units, and measure hearing range.")], M, 6.05, 12.3, 0.95);

pres.writeFile({ fileName: OUT }).then(f => console.log("wrote", f));
