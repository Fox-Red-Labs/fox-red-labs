// Alarm listener on the LP-MSPM0G5187: methods, memory, CPU, M0+ vs M33 vs NPU, tests, results, path to a product.
// Every number comes from mspm0g5187/alarm/results/, data/results/*.csv or the build map; estimates say so.
// Build: NODE_PATH=../../../tools/deck/node_modules node build.js [out.pptx]
const pptxgen = require("pptxgenjs");
const OUT = process.argv[2] || "alarm-listener.pptx";
// Fox Red Labs brand: Paper background, Ink text, Fox Red the only accent, warm neutrals for data.
const INK = "161312", STONE = "6B625B", LINE = "D9D1C5", PAPER = "F4EFE6", MIST = "F7F3EC", ACC = "B42418",
  OK = "2E6B3F", AMBER = "8E1C13", RED = "8E1C13", NPU = "B42418", CPU = "4A423C", ISR = "8C827A";
const LOGO = require("path").join(__dirname, "../../../brand/kit/png/horizontal-red-2000.png");
const W = 13.333, M = 0.5, F = "Barlow";
const DISCLOSE = "Fox Red Labs is an independent project. Its author works at Texas Instruments. It is not affiliated with, sponsored by, or endorsed by Texas Instruments.";
const FOOT = "Fox Red Labs. Bench results 12 to 13 Oct 2026. LP-MSPM0G5187 Rev A, MSPM0 SDK 2.11, TI Arm Clang 5.1.1, TI NNC 2.1.2. Measured unless marked estimate.";
const pres = new pptxgen(); pres.layout = "LAYOUT_WIDE"; pres.title = "Alarm listener on the MSPM0G5187";
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

// 1. Title and the answer
frame("It learns your smoke alarm and tells you when it sounds",
  "The NPU (TinyEngine) in an MSPM0G5187 listens through the LaunchPad kit's own microphone, tells a smoke alarm from a CO alarm, and ignores everything else.");
const kpi = (x, big, small, col = INK) => { box(x, 1.6, 3.95, 1.55, MIST); txt([T(big, { fontSize: 34, bold: true, color: col, breakLine: true }), T(small, { fontSize: 12, color: INK })], x + 0.2, 1.72, 3.55, 1.35); };
kpi(M, "0 false alarms", "in 3.4 hours of a real room overnight (12,094 decisions), and in 1.6 hours of simulated house sound", OK);
kpi(M + 4.15, "162 of 167", "simulated alarms caught when the board sits beside the alarm (97%)");
kpi(M + 8.3, "80 kB + 30 kB", "flash and RAM for the whole listener, on a chip with 128 kB and 32 kB");
sec("In one minute", M, 3.55, 6);
txt([
  T("1. Stick it beside an ordinary smoke or CO alarm. Press its button, then the alarm's test button: it learns that alarm's tone and how loud it is up close.", { breakLine: true }),
  T(" ", { fontSize: 6, breakLine: true }),
  T("2. From then on it listens all the time. When that alarm sounds it says which: smoke or CO. It already posts to Home Assistant.", { breakLine: true }),
  T(" ", { fontSize: 6, breakLine: true }),
  T("3. It ignores whistles, TV, music, ringtones, oven and microwave beeps, and the same alarm pattern from a TV across the room.", {}),
], M, 3.95, 7.2, 2.9, { fontSize: 14, paraSpaceAfter: 8 });
note([T("Status", { bold: true, color: AMBER, fontSize: 13, breakLine: true }),
  T("Proven on the board with the standard alarm patterns: fed in over USB, and played through the air from a laptop speaker. ", { fontSize: 11.5 }),
  T("Not yet tried: a real smoke alarm's horn. That is the next test.", { fontSize: 11.5, bold: true })], 8.2, 3.6, 4.6, 1.9);

// 2. The idea: three clues
frame("Three clues tell your alarm apart from every other sound", "The worry with a listening device is false alarms. Each clue below rules out a different kind of mistake.");
sec("Clue 1: the pattern is set by the fire code (NFPA 72, each time within 10%)", M, 1.5, 12);
const tl = (y, label, beeps, col) => {
  txt([T(label, { bold: true, fontSize: 11 })], M, y, 2.3, 0.5, { valign: "middle" });
  const X0 = 2.9, PX = 0.62;
  rect(X0, y + 0.37, 12 * PX, 0.02, LINE);
  beeps.forEach(([a, b]) => rect(X0 + a * PX, y + 0.05, Math.max((b - a) * PX, 0.035), 0.32, col));
};
const t3 = []; for (let k = 0; k < 3; k++) for (let i = 0; i < 3; i++) { const a = k * 4 + i; if (a < 12) t3.push([a, a + 0.5]); }
const t4 = []; for (let k = 0; k < 3; k++) for (let i = 0; i < 4; i++) { const a = k * 5.8 + i * 0.2; if (a < 12) t4.push([a, a + 0.1]); }
tl(1.95, "Smoke: temporal-3", t3, RED);
tl(2.55, "CO: temporal-4", t4, NPU);
for (let sx = 0; sx <= 12; sx += 2) txt([T(`${sx} s`, { fontSize: 9, color: STONE })], 2.9 + sx * 0.62 - 0.15, 3.0, 0.5, 0.25);
txt([T("Three 0.5 s beeps, then 1.5 s quiet", { fontSize: 10.5, color: STONE })], 10.55, 1.95, 2.6, 0.45, { valign: "middle" });
txt([T("Four 0.1 s pulses, then 5 s quiet", { fontSize: 10.5, color: STONE })], 10.55, 2.55, 2.6, 0.45, { valign: "middle" });
const clue = (x, title, body, beats) => { box(x, 3.55, 3.95, 2.7); txt([T(title, { bold: true, fontSize: 15, color: ACC, breakLine: true }), T(body, { fontSize: 12.5, breakLine: true }), T("Rules out: ", { bold: true, fontSize: 12.5 }), T(beats, { fontSize: 12.5 })], x + 0.2, 3.7, 3.55, 2.95, { paraSpaceAfter: 8 }); };
clue(M, "Pattern", "A neural net on the NPU spots alarm-like sound; a timing check then insists on the exact pattern above. Both must agree.", "oven timers, microwaves, alarm clocks, phones, whistles, and beeps with the wrong timing.");
clue(M + 4.15, "Tone", "Pressing the test button once teaches it the horn's pitch (for example 3,162 Hz). Later alarms must match within 4%.", "other alarms and look-alike beeps at a different pitch.");
clue(M + 8.3, "Loudness", "It also learns how loud the alarm is from right beside it. A sound more than 10 dB quieter is not that alarm.", "the same pattern from a TV, a video, or a neighbour's alarm through a wall.");

// 3. How it works
frame("How it works: from microphone to notification", "Everything runs on one MSPM0G5187 (Arm Cortex-M0+ at 80 MHz with the TinyEngine NPU). Colour shows which part of the chip does the work.");
const steps = [
  ["Microphone", "ICS-40720 into a PCM1809 audio converter on the LaunchPad. I2S at 47.6 kHz.", MIST, "hardware"],
  ["Filter and slow down", "60-tap filter, keep 1 sample in 3: 15.9 kHz. Runs in the DMA interrupt every 1 ms.", ISR, "interrupt"],
  ["Sound features", "Every 64.5 ms: two 512-point FFTs give 13 numbers (band levels, main tone, how tonal).", CPU, "CPU"],
  ["Neural net", "Every 1.03 s: the last 8.3 s (16 x 128 numbers) in, other / smoke / CO out. 13.8k weights.", NPU, "NPU"],
  ["Timing check", "Smoke: 3 beeps and a long gap. CO: two pulse groups 5.8 s apart.", CPU, "CPU"],
  ["Learned gate", "Tone within 4% and loudness within 10 dB of what S2 learned. Kept in data flash.", CPU, "CPU"],
];
const BW = 1.9, BG = 0.17;
steps.forEach(([h, b, col, tag], i) => {
  const x = M + i * (BW + BG);
  box(x, 1.6, BW, 2.3);
  s.addShape(pres.shapes.RECTANGLE, { x, y: 1.6, w: BW, h: 0.38, fill: { color: col === MIST ? "6B625B" : col }, line: { color: col === MIST ? "6B625B" : col } });
  txt([T(tag, { bold: true, color: PAPER, fontSize: 10 })], x + 0.1, 1.64, BW - 0.2, 0.3, { valign: "middle" });
  txt([T(h, { bold: true, fontSize: 13, breakLine: true }), T(b, { fontSize: 11, color: STONE })], x + 0.1, 2.08, BW - 0.2, 1.8);
  if (i < steps.length - 1) arrow(x + BW + 0.01, 2.75, x + BW + BG - 0.01, 2.75);
});
box(M, 4.45, 3.3, 1.0, MIST); txt([T("Report", { bold: true, fontSize: 12.5, breakLine: true }), T("\"ALARM smoke\" or \"ALARM co\" on the serial port", { fontSize: 10, color: STONE })], M + 0.12, 4.52, 3.1, 0.9);
arrow(M + 3.3, 4.95, M + 3.75, 4.95);
box(M + 3.75, 4.45, 3.3, 1.0, MIST); txt([T("Mac bridge, then Home Assistant", { bold: true, fontSize: 12.5, breakLine: true }), T("sensor.alarm_listener: quiet, smoke, co, learning; plus a notification", { fontSize: 10, color: STONE })], M + 3.87, 4.52, 3.1, 0.9);
arrow(M + 7.05, 4.95, M + 7.5, 4.95);
box(M + 7.5, 4.45, 4.8, 1.0, PAPER, AMBER); txt([T("In a product: a radio instead of the Mac", { bold: true, fontSize: 12.5, breakLine: true }), T("Zigbee or Thread straight to the hub (slide 12)", { fontSize: 10, color: STONE })], M + 7.62, 4.52, 4.6, 0.9);
note([T("Why two stages? ", { bold: true }), T("The net is good at \"this sounds like an alarm\" even in noise. Simple rules are good at \"the timing is exactly the standard\". Alone, the net made 7 false alarms in 1.6 hours of simulated house sound; with the timing check, 0.")], M, 5.75, 12.3, 1.05);

// 4. Data sources
frame("Where the data came from", "No real alarm recordings were used to train it. The model learned from the standard's timing, drawn in code, mixed into realistic room sound.");
tbl([hdr(["Source", "What we used it for", "Detail"]),
  [c("NFPA 72 (fire alarm code), sections 18.4.2 and 18.4.3.2", { bold: true }), c("Exact smoke (temporal-3) and CO (temporal-4) timing"), c("Training alarms vary up to 12%; look-alikes are off by 35% or more")],
  [c("TI MSPM0 SDK 2.11 examples", { bold: true }), c("Microphone settings (device_uac_microphone_i2s); NPU start-up (glass_break_detection_ai_npu, hello_world_ai_npu)"), c("Public SDK; the I2S clock and DMA settings unchanged")],
  [c("Real sounds on this Mac", { bold: true }), c("193 decoy sources, 49 minutes: 151 ringtones and alert tones, 40 synthetic speech voices, 2 music tracks"), c("Split by source: 43 sources held out and never trained on")],
  [c("Look-alikes drawn in code", { bold: true }), c("16 kinds: oven timer, backup beeper, microwave, alarm clock, alarm patterns with wrong timing, chirp, whistle, kettle, doorbell, phone, birds, clinks, claps"), c("host/synth.py")],
  [c("Generated scenes", { bold: true }), c("Room noise, mains hum, echo, microphone colouring, 0 to 6 decoys, the alarm 0 to 60 dB above the room"), c("3,500 training scenes of 40 s (39 h); 600 held-out scenes (6.7 h)")],
  [c("The board's own microphone", { bold: true }), c("A quiet-room recording (-79.6 dBFS) laid under the silent board tests"), c("No dropped samples in 10 s")],
  [c("Tools", { bold: true }), c("PyTorch 2.2.2; TI tinyml_torchmodelopt 1.5.0 (8-bit training for the NPU); TI NNC 2.1.2 (compiler) on GitHub; TI Arm Clang 5.1.1"), c("Training took 32 minutes on an Intel Mac CPU")],
], { y: 1.5, w: 12.3, colW: [3.0, 6.0, 3.3], rowH: 0.62 });

// 5. How we tested
frame("How we tested: seven rungs, each harder to fool than the last", "Each rung checks something the rung below cannot. The last rung, a real horn, is still to do.");
const rungs = [
  ["1", "Board code against the Mac's reference", "The C feature code, input builder and timing check give the same numbers as the Python specification.", "Features 0.0 dB median apart; input and timing identical on 4,000 windows", OK],
  ["2", "Simulated rooms", "600 held-out scenes; decoy sounds the model never heard.", "0 false alarms in 1.6 h; 162 of 167 alarms beside the board", OK],
  ["3", "NPU self-test on the board", "6 held-out windows through the compiled model on the NPU.", "6 of 6 match the reference model", OK],
  ["4", "Board, silent end to end", "Audio sent over USB onto real room sound: learn, alarm, 18 decoys, TV, other tone, CO.", "Every case as expected", OK],
  ["5", "Board, through the air", "A Mac speaker plays 21 items at the board, 8:23 to 8:31.", "Smoke alarmed; 14 of 16 decoys quiet; 2 CO look-alikes fixed after", AMBER],
  ["6", "Board, real room overnight", "Nothing played. 3.4 hours of the house.", "0 alarms in 12,094 decisions", OK],
  ["7", "A real smoke alarm's horn", "Learn it with its test button, then test presses.", "Next", STONE],
];
rungs.forEach(([k, h, what, res, col], i) => {
  const y = 1.45 + i * 0.77;
  s.addShape(pres.shapes.OVAL, { x: M, y: y + 0.1, w: 0.5, h: 0.5, fill: { color: col }, line: { color: col } });
  txt([T(k, { bold: true, color: PAPER, fontSize: 14 })], M, y + 0.1, 0.5, 0.5, { align: "center", valign: "middle" });
  txt([T(h, { bold: true, fontSize: 12, breakLine: true }), T(what, { fontSize: 10, color: STONE })], M + 0.7, y + 0.04, 6.3, 0.7);
  txt([T(res, { fontSize: 11, bold: true, color: col === STONE ? STONE : col })], M + 7.2, y + 0.04, 5.1, 0.7, { valign: "middle" });
});

// 6. Simulation results
frame("Results in simulation: no false alarms, and it catches the alarm beside it", "600 held-out scenes of 40 s, sounds the model never trained on. Same decision rule as the board: 6 of the last 8 answers agree, plus the timing check.");
sec("False alarms in 1.6 hours of alarm-free sound", M, 1.5, 5.6);
vbars(M + 0.5, 2.35, 4.6, 2.2, [["Neural net alone", 7, RED], ["Net + timing check", 0, OK]], { max: 7 });
sec("Alarms caught, by how loud the alarm is over the room", 6.3, 1.5, 6.6);
vbars(6.3, 2.35, 6.5, 2.2, [["0 to 10 dB", 64, CPU, "48/75"], ["10 to 20 dB", 69, CPU, "63/91"], ["20 to 40 dB", 85, CPU, "113/133"], ["over 40 dB", 94, CPU, "116/124"], ["Beside the alarm", 97, OK, "162/167"]], { max: 100 });
txt([T("bar height = share caught", { fontSize: 9, color: STONE })], 6.3, 1.85, 4, 0.3);
tbl([hdr(["Measure", "Result"]),
  [c("Window accuracy, 8-bit model (other / smoke / CO)"), c("94.3%")],
  [c("All alarms caught, any loudness"), c("340 of 423 (80%); 2 called the wrong type")],
  [c("Beside the alarm (20 dB over the room, louder than any other sound)"), c("162 of 167 (97%)", { bold: true, color: OK })],
  [c("Time to alarm once a full pattern is in view"), c("5.2 s median")],
], { x: M, y: 5.1, w: 7.6, colW: [5.2, 2.4], rowH: 0.36 });
note([T("Why far, quiet alarms are missed more: ", { bold: true }), T("buried under louder sounds, the timing check cannot see clean beeps. A product sits beside the alarm, which is the green bar.")], 8.4, 5.1, 4.4, 1.8);

// 7. Board results
frame("Results on the board: every case right once the CO rule was fixed", "Silent tests send audio over USB into the board; the through-the-air test plays it from a laptop speaker at the microphone.");
tbl([hdr(["Test", "Expected", "What the board said", "Pass"]),
  [c("Learn a smoke alarm (S2, then the alarm)"), c("Learned"), c("Learned smoke, 3,162 Hz; saved; still there after a reboot"), yes()],
  [c("The same alarm"), c("Alarm, smoke"), c("ALARM smoke"), yes()],
  [c("CO pattern from the same horn"), c("Alarm, CO"), c("ALARM co"), yes()],
  [c("18 decoys (timers, beepers, whistle, kettle, doorbell, phone, speech, music...)"), c("Nothing"), c("Nothing"), yes()],
  [c("Same alarm, 31 dB quieter (a TV)"), c("Ignored"), c("Ignored: too quiet"), yes()],
  [c("An alarm at 3,600 Hz"), c("Ignored"), c("Ignored: tone differs"), yes()],
  [c("CO look-alikes: 3 pulses; 1.5 s gap"), c("Nothing"), c("Nothing (after the fix)"), yes()],
  [c("Through the air: smoke pattern", { bold: true }), c("Alarm, smoke"), c("ALARM smoke"), yes()],
  [c("Through the air: 16 decoys, nothing learned", { bold: true }), c("Nothing"), c("14 quiet; the 2 CO look-alikes alarmed. Rule fixed, retested silently"), nope("2 missed")],
  [c("Through the air, with the alarm learned (replayed from the board's log)", { bold: true }), c("Quiet copy and other tone ignored"), c("Both ignored; 0 of 16 decoys"), yes()],
  [c("Real room overnight, 3.4 h", { bold: true }), c("Nothing"), c("0 alarms in 12,094 decisions"), yes()],
], { y: 1.45, w: 12.3, colW: [4.6, 2.0, 4.6, 1.1], rowH: 0.43 });

// 8. Memory
frame("Memory: plenty of flash left, RAM nearly full", "From the linker map of the listener image. The chip has 128 kB of flash and 32 kB of RAM.");
const stack = (y, title, total, parts) => {
  sec(title, M, y, 8);
  let x = M; const SW = 12.3 / total;
  parts.forEach(([name, kb, col]) => { rect(x, y + 0.4, kb * SW, 0.6, col); if (kb * SW > 0.9) txt([T(`${kb}`, { color: ["F7F3EC", "D9D1C5", "C8BFB5", "E5DED3", "A39A92"].includes(col) ? INK : "F7F3EC", bold: true, fontSize: 11 })], x + 0.05, y + 0.4, kb * SW - 0.1, 0.6, { align: "center", valign: "middle" }); x += kb * SW; });
  s.addShape(pres.shapes.RECTANGLE, { x: M, y: y + 0.4, w: 12.3, h: 0.6, fill: { type: "none" }, line: { color: STONE, width: 1 } });
  let lx = M;
  parts.forEach(([name, kb, col]) => { rect(lx, y + 1.1, 0.16, 0.16, col); s.addShape(pres.shapes.RECTANGLE, { x: lx, y: y + 1.1, w: 0.16, h: 0.16, fill: { type: "none" }, line: { color: LINE, width: 0.5 } }); const lab = `${name} ${kb}`; txt([T(lab, { fontSize: 9.5 })], lx + 0.22, y + 1.06, lab.length * 0.075 + 0.2, 0.25, { valign: "middle" }); lx += lab.length * 0.075 + 0.55; });
};
stack(1.45, "Flash: 80.6 of 128 kB used", 128, [["Model", 21.6, NPU], ["Test vectors", 24.6, "D9D1C5"], ["Listener", 17.9, CPU], ["FFT", 7.2, "A39A92"], ["Features", 2.7, "C8BFB5"], ["Drivers, runtime", 6.6, "6B625B"], ["Free", 47.4, MIST]]);
stack(2.85, "RAM: 30.4 of 32 kB used", 32, [["Model input", 8.2, NPU], ["Model scratch", 4.5, "8E1C13"], ["Audio buffer", 6.1, ISR], ["Feature history", 3.3, CPU], ["FFT", 3.0, "A39A92"], ["Stack", 2.0, "6B625B"], ["Other", 3.3, "C8BFB5"], ["Free", 1.6, MIST]]);
tbl([hdr(["Flash item", "kB", "What it is"]),
  [c("Model (weights and NPU code)"), c("21.6"), c("TI NNC output for 13.8k weights")],
  [c("Test vectors"), c("24.6"), c("Self-test only: delete in a product")],
  [c("Listener"), c("17.9"), c("Decisions, timing check, flash, UART")],
  [c("FFT, features"), c("9.9"), c("CMSIS-DSP 512-point FFT plus our code")],
  [c("Drivers, runtime"), c("6.6"), c("TI DriverLib, C library, soft float")],
], { y: 4.45, w: 6.0, colW: [2.2, 0.7, 3.1], rowH: 0.33 });
tbl([hdr(["RAM item", "kB", "What it is"]),
  [c("Model input"), c("8.2"), c("16 x 128 numbers as float")],
  [c("Model scratch"), c("4.5"), c("NPU layer buffers")],
  [c("Audio buffer"), c("6.1"), c("Last 97 ms of sound")],
  [c("Feature history"), c("3.3"), c("Last 8.3 s of features")],
  [c("FFT, stack, other"), c("8.3"), c("FFT 3.0, stack 2.0, the rest 3.3")],
], { x: 6.8, y: 4.45, w: 6.0, colW: [2.0, 0.7, 3.3], rowH: 0.33 });
txt([T("Easy wins: ", { bold: true, color: OK }), T("drop the test vectors (24.6 kB flash) and feed the NPU 8-bit numbers instead of float (about 6 kB RAM back).")], M, 6.55, 12.3, 0.45, { fontSize: 12 });

// 9. CPU
frame("CPU: the AI is cheap; slowing the audio down is the biggest job", "Cycle counts on the board at 80 MHz (SysTick). Share of the CPU = time per run x runs per second.");
sec("Where the 80 MHz CPU goes, each second", M, 1.45, 5);
[["Filter and slow down (interrupt)", 43.0, ISR], ["Sound features", 11.0, CPU], ["Neural net call", 1.9, NPU], ["Idle", 44.1, "E5DED3"]].forEach(([lab, v, col], i) => {
  const y = 1.95 + i * 0.62;
  txt([T(lab, { fontSize: 10.5 })], M, y - 0.05, 4.8, 0.25, { valign: "middle" });
  rect(M, y + 0.2, 3.0 * v / 50, 0.3, col);
  txt([T(`${v}%`, { bold: true, fontSize: 12.5 })], M + 3.0 * v / 50 + 0.1, y + 0.18, 1.0, 0.34, { valign: "middle" });
});
txt([T("Bars on one scale; idle is time free for more work or sleep.", { fontSize: 9.5, color: STONE })], M, 4.45, 4.7, 0.4);
tbl([hdr(["Stage", "Cycles per run", "Time", "How often", "CPU share"]),
  [c("Filter and slow down (DMA interrupt)"), c("34,410"), c("0.43 ms"), c("every 1 ms"), c("43%", { bold: true, color: ISR })],
  [c("Sound features (two FFTs)"), c("568,000"), c("7.1 ms"), c("every 64.5 ms"), c("11%")],
  [c("Neural net call on the NPU"), c("1,588,000"), c("19.9 ms"), c("every 1.03 s"), c("1.9%")],
  [c("Timing check"), c("74,000"), c("0.93 ms"), c("every 1.03 s"), c("0.1%")],
  [c("Building the model input"), c("12,000"), c("0.15 ms"), c("every 1.03 s"), c("0.01%")],
], { x: 5.6, y: 1.5, w: 7.2, colW: [2.65, 1.2, 0.95, 1.25, 1.15], rowH: 0.42 });
note([T("Two findings for an engineer", { bold: true, color: ACC, breakLine: true }),
  T("1. The filter that turns 47.6 kHz into 15.9 kHz takes 43% of the chip. The PCM1809 can sample at 8 to 192 kHz itself (data sheet); clocking it at 16 kHz should remove almost all of this. Not tried yet.", { breakLine: true }),
  T("2. The 19.9 ms net call is mostly the CPU converting 2,048 float inputs to 8-bit in software, not the NPU. Estimate: about 6 ms is NPU work (the 8k occupancy model, with 1/16 the inputs and 1/3 the arithmetic, takes 2.1 ms). 8-bit input straight from the feature code should cut the call to a third or less.", { breakLine: true }),
  T("Live, the call reads 31 ms because the audio interrupt keeps cutting in.", { color: STONE })], M, 4.95, 12.3, 1.95);

// 10. M0+ vs M33 vs NPU
frame("M0+, M33 and the NPU on the same kind of model", "Left: the same 8k-weight room-occupancy model, measured on each board (data/results). Right: this alarm model, 2.75 times the arithmetic (434k multiply-adds), estimated from the left.");
sec("8k occupancy model, ms per answer (measured; log scale)", M, 1.45, 6.4);
hbars(M, 1.85, 6.4, [["CC2340R5 M0+ 48 MHz, radio", 63.2, CPU], ["MSPM0G5187 M0+ 80 MHz", 35.3, CPU], ["CC2755P10 M33 96 MHz, radio", 10.5, "8C827A"], ["MSPM33C321A M33 160 MHz", 7.38, "8C827A"], ["AM13E230 M33 200 MHz", 5.06, "8C827A"], ["MSPM0G5187 NPU 80 MHz", 2.12, NPU], ["AM13E230 NPU 200 MHz", 0.63, NPU]], { max: 100, log: 0.3, labelW: 2.7, rowH: 0.52, unit: " ms" });
txt([T("Blue: M0+ CPU. Light blue: M33 CPU. Purple: NPU.", { fontSize: 9.5, color: STONE })], M, 5.5, 6.4, 0.3);
tbl([hdr(["Chip", "Alarm model, ms", "CPU at 1 per second"]),
  [c("MSPM0G5187 NPU"), c("19.9 measured", { bold: true }), c("2%")],
  [c("  of which NPU work"), c("about 6 (est.)"), c("")],
  [c("AM13E230 NPU"), c("about 1.7 (est.)"), c("under 1%")],
  [c("AM13E230 M33"), c("about 14 (est.)"), c("1%")],
  [c("MSPM33C321A M33"), c("about 20 (est.)"), c("2%")],
  [c("CC2755P10 M33 (radio)"), c("about 29 (est.)"), c("3%")],
  [c("MSPM0G5187 CPU only"), c("about 97 (est.); did not fit in RAM"), c("10%")],
  [c("CC2340R5 M0+ (radio)"), c("about 174 (est.)"), c("17%")],
], { x: 7.1, y: 1.85, w: 5.7, colW: [2.2, 2.2, 1.3], rowH: 0.4 });
sec("This alarm model (434k multiply-adds)", 7.1, 1.45, 5.7);
note([T("Takeaway: ", { bold: true }), T("on this kind of model the NPU makes the M0+ about 17 times faster than its own CPU and faster than an M33 at twice the clock. At one answer a second, an M33 radio chip could also run it in software; the NPU leaves room for a bigger model or deep sleep between answers.")], M, 5.95, 12.3, 0.95);

// 11. What we learned
frame("What we learned", null);
const lessons = [
  ["Rules plus a net beat a net alone", "The timing check took simulated false alarms from 7 to 0 and cost about a tenth of the far, quiet alarms."],
  ["Loudness is the cheap TV filter", "Learning how loud the alarm is up close rejected the same pattern 22 dB quieter. It works because the device sits beside the alarm."],
  ["The live test found what simulation missed", "Two CO look-alikes alarmed through the air. Fix: CO needs two pulse groups 5.8 s apart, as real alarms repeat."],
  ["The AI was not the CPU hog", "The audio filter takes 43%. The net call is 2%, and most of that is float conversion on the CPU."],
  ["Hardware sharing bites", "The microphone's clock pins are also on header J20; a radio wired there had to come off. No software workaround worked."],
  ["Flash writes need care", "This data flash took one 64-bit write; two writes failed and a 128-bit write left a record that stopped the board at boot."],
  ["Learn and clear share one button", "Pressing S2 while an alarm is on clears it, so learning got its own command."],
  ["A laptop speaker is not a horn", "Every alarm so far is the standard pattern from a speaker or over USB. A real alarm is the proof still owed."],
];
lessons.forEach(([h, b], i) => {
  const col = i % 2, row = Math.floor(i / 2), x = M + col * 6.2, y = 1.0 + row * 1.5;
  box(x, y, 6.0, 1.35);
  txt([T(h, { bold: true, fontSize: 15, color: ACC, breakLine: true }), T(b, { fontSize: 13 })], x + 0.2, y + 0.15, 5.6, 1.1, { paraSpaceAfter: 4 });
});

// 12. Toward a product
frame("Toward a product: system design and what is left", "A stick-on listener needs a radio, a power plan and real-alarm data. One-chip and two-chip designs both fit; the numbers say which trade-off you buy.");
sec("System", M, 1.45, 6);
const node = (x, y, w, h, t, sub, col = ACC) => { box(x, y, w, h, PAPER, col); txt([T(t, { bold: true, fontSize: 11.5, breakLine: true }), T(sub, { fontSize: 9.5, color: STONE })], x + 0.1, y + 0.06, w - 0.2, h - 0.1); };
node(M, 1.85, 1.6, 0.95, "Microphone", "MEMS mic + audio ADC at 16 kHz", "6B625B");
arrow(M + 1.6, 2.32, M + 1.85, 2.32);
node(M + 1.85, 1.85, 2.0, 0.95, "MSPM0G5187", "listen, NPU, decide", NPU);
arrow(M + 3.85, 2.32, M + 4.1, 2.32);
node(M + 4.1, 1.85, 1.75, 0.95, "Radio chip", "Zigbee or Thread", ACC);
arrow(M + 4.98, 2.8, M + 4.98, 3.1);
node(M + 4.1, 3.1, 1.75, 0.8, "Home hub", "Home Assistant or another Zigbee hub", "6B625B");
arrow(M + 4.1, 3.5, M + 3.4, 3.5);
node(M + 1.85, 3.1, 1.55, 0.8, "Phone", "\"Smoke alarm\" / \"CO alarm\"", OK);
tbl([hdr(["Design", "For", "Against"]),
  [c("A. MSPM0G5187 + CC2340R5 (Zigbee) on UART", { bold: true }), c("NPU headroom; radio stack untouched; each part already run here"), c("Two chips, two firmware images")],
  [c("B. One CC2755P10 (M33 radio), model in software", { bold: true }), c("One chip; about 29 ms of CPU a second (est.)"), c("No NPU; RAM shared with the radio stack, not checked")],
  [c("C. MSPM0G5187 + Wi-Fi chip (Matter)", { bold: true }), c("No hub needed"), c("Wi-Fi draws more power; likely plug-in")],
], { y: 4.15, w: 6.0, colW: [2.3, 2.0, 1.7], rowH: 0.62 });
sec("Work left before a product", 6.8, 1.45, 6);
tbl([hdr(["Work", "Why"]),
  [c("Record real alarms: 10+ brands, 520 Hz low-tone units, combination smoke and CO", { bold: true }), c("Trained only on the standard's timing; horns differ in pitch and wobble")],
  [c("Doorbell class", { bold: true }), c("Asked for; not started. Each doorbell differs, so learn-by-example fits")],
  [c("Audio at 16 kHz in the converter; 8-bit model input", { bold: true }), c("Frees 43% of the CPU and about 6 kB RAM; makes room to sleep")],
  [c("Power budget", { bold: true }), c("Always-on mic, converter and MCU not measured yet; decides battery or plug-in")],
  [c("Enclosure, distance and mic port", { bold: true }), c("Loudness gate assumes a fixed spot beside the alarm")],
  [c("Field logging and updates", { bold: true }), c("Count real false alarms; ship model fixes over the air")],
  [c("Positioning and certification", { bold: true }), c("A supplemental notifier, not a listed alarm (UL 217 smoke, UL 2034 CO); radio certification")],
], { x: 6.8, y: 1.85, w: 6.0, colW: [3.0, 3.0], rowH: 0.62 });
note([T("Next three steps: ", { bold: true, color: ACC }), T("1. Test a real smoke alarm (one button press, then test presses). 2. Move the 16 kHz sampling and 8-bit input onto the board and re-measure. 3. Wire the CC2340R5 Zigbee link and report to Home Assistant without the Mac.")], M, 6.15, 12.3, 0.8);

pres.writeFile({ fileName: OUT }).then(f => console.log("wrote", f));
