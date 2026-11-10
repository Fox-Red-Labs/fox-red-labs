// Sample the LP-AM243 through the debug port's system view, with the core left running. Never connects to a core, so nothing halts.
// /Applications/ti/ccs2101/ccs/scripting/run.sh am243x/tools/live_probe.js <seconds> <out.csv>. LIVE_JSON=<file> also writes the state for the chip view every 100 ms.
//
// Fast path (about 800 times a second): GPIO0 output data (pin 22, the LED) and TIMER8's counter, for the edge timeline.
// Slow path, one read per four fast reads, round robin, each value refreshed about every 0.4 s:
//   power state      PSC module status of every LPSC the chip map lists (MAIN PSC 0x400000, MCU PSC 0x4000000; state 3 is enabled)
//   pad mux          the pad registers of the pins the chip map lists as used
//   block enables    UART line control, I2C control, timer control, GPIO direction: the bit the driver sets when it starts the block
//   timers           TIMER6 and TIMER8 control and counter
//   program block    the busy program's status block in TCMB (am243x/bench/chipview/busy/busy.c): per-task load, total load, counters,
//                    and the interrupt controller's enable mask, copied there because the VIM is core-local
// LIVE_LOG=<file> also appends the whole state once a second as one JSON line, for checks against known values.
// Core load also comes from the frl_load.h idle beacon (TCMB 0x78107FF0), read every 100 ms.
const fs = require("fs"), { join } = require("path");
const out = process.argv[process.argv.length - 1], secs = Number(process.argv[process.argv.length - 2]);
const ds = initScripting(); ds.setScriptingTimeout(30000);
const { nonDebugCores } = ds.configure(join(__dirname, "..", "AM243x_LP_" + (process.env.AM243X_SERIAL || "S24L0096") + ".ccxml"));
const d = ds.openSession(nonDebugCores.find(c => /CS_DAP_0/.test(c)));
d.target.connect();
const rd = a => { try { return Number(d.memory.readOne("0x" + a.toString(16) + "@System_View", 32)) >>> 0; } catch (e) { return null; } };
const rows = []; const t0 = Date.now(), json = process.env.LIVE_JSON;
let last = null, edges4 = [], tw = 0, lastLog = -1000;
// Core 0 load from a beacon in its TCMB (the R5F has no PC sampling register). "FRLD" (frl_load.h, bare-metal): time asleep counted around WFI.
// "FRLC" (frl_pmu_load.h, RTOS): cycle counter that stops in WFI, stamped with the time in an interrupt, read with a sequence check.
const B = 0x78107FF0, magic = rd(B), pmu = magic === 0x434C5246, hasBeacon = pmu || magic === 0x444C5246;
const beacon = () => { if (!hasBeacon) return null;
  if (!pmu) return [rd(B + 4), rd(B + 8)];
  for (let i = 0; i < 6; i++) { const s = rd(B + 12), t = rd(B + 4), c = rd(B + 8); if (s === rd(B + 12) && !(s & 1)) return [t, c]; } return null; };
const hist = []; let load = null;   // beacon samples, one per 100 ms; load is the awake share over the last second so a 100 ms duty cycle averages out
const awake = (b, p) => { const dt = (b[0] - p[0]) >>> 0, dc = (b[1] - p[1]) >>> 0;
  return pmu ? Math.round(1000 * dc / (800 * dt)) / 10 : Math.round(1000 * (1 - dc / dt)) / 10; };

// The slow set is built from the chip map, so the probe reads what the page draws.
const map = JSON.parse(fs.readFileSync(process.env.CHIP_MAP || join(__dirname, "../../docs/site-redesign/concepts/am243-chip-map.json"), "utf8"));
const slow = [], S = { psc: {}, pads: {}, en: {}, tim: {}, prog: {}, vim: new Array(16).fill(0), gpio: {} };
const add = (get, set) => slow.push([get, set]);
const lp = {};
for (const b of Object.values(map.blocks)) if (b.lpsc) lp[b.lpsc.id + ":" + b.lpsc.mdstat] = 1;
for (const k of Object.keys(lp)) { const [id, a] = k.split(":").map(Number); add(a, v => { S.psc[a] = v == null ? null : v & 0x3f; }); }
for (const [n, p] of Object.entries(map.pads)) add(parseInt(p.reg, 16), v => { S.pads[n] = v; });
for (const [n, a] of [["uart0_lcr", 0x280000c], ["uart1_lcr", 0x281000c], ["i2c0_con", 0x200000a4], ["i2c1_con", 0x200100a4], ["tim6_tclr", 0x2460038], ["tim8_tclr", 0x2480038], ["gpio0_dir", 0x600010], ["gpio0_in", 0x600020]])
  add(a, v => { S.en[n] = v; });
add(0x246003c, v => { S.tim.t6 = v; });
const ST = 0x78107F00;
let progOk = false, names = [];
const w = (i, f) => add(ST + 4 * i, v => f(v));
w(0, v => { progOk = v === 0x424C5246; S.prog.ok = progOk; });
const sc = { tick: 2, os_load: 3, tmr: 4, i2c: 5, i2c_err: 6, eeprom: 7, led: 8, console: 9, crunch: 10, stats: 11, ntask: 12 };
for (const [k, i] of Object.entries(sc)) w(i, v => { S.prog[k] = v; });
for (let t = 0; t < 5; t++) {
  for (let q = 0; q < 3; q++) w(16 + 4 * t + q, v => { (names[t] = names[t] || [0, 0, 0])[q] = v; });
  w(19 + 4 * t, v => { (S.prog.tasks = S.prog.tasks || [])[t] = v == null ? null : v / 100; });
}
for (let g = 0; g < 16; g++) w(40 + g, v => { S.vim[g] = v; });
let si = 0, ni = 0;
const nameOf = n => { if (!n) return ""; let s = ""; for (const x of n) for (let i = 0; i < 4; i++) { const c = (x >>> (8 * i)) & 255; if (c) s += String.fromCharCode(c); } return s; };

while (Date.now() - t0 < secs * 1000) {
  const v = rd(0x600014), t = rd(0x248003c), now = Date.now() - t0, bit = (v / 2 ** 22) & 1;
  rows.push([now, bit, t]);
  if (++ni % 4 === 0 && slow.length) { const [a, set] = slow[si++ % slow.length]; set(typeof a === "function" ? a() : rd(a)); }
  if (json) {
    if (bit !== last) { edges4.push([now, bit]); last = bit; }
    if (now - tw > 100) { tw = now;
      if (hasBeacon) { const b = beacon();
        if (b) { hist.push(b); const ref = hist.find(h => ((b[0] - h[0]) >>> 0) <= 1000000 && ((b[0] - h[0]) >>> 0) > 500000);
          if (ref) load = awake(b, ref); while (hist.length > 14) hist.shift(); } }
      edges4 = edges4.filter((e, i) => e[0] > now - 8000 || i === edges4.length - 1);
      const prog = Object.assign({}, S.prog);
      if (prog.tasks) prog.tasks = prog.tasks.map((l, i) => ({ name: nameOf(names[i]), load: l }));
      const vim = []; S.vim.forEach((m, g) => { for (let b = 0; b < 32; b++) if ((m >>> b) & 1) vim.push(32 * g + b); });
      const doc = { now, pin: bit, edges: edges4, rate: Math.round(rows.length / (now / 1000 || 1)), load,
        psc: S.psc, pads: S.pads, en: S.en, tim: { t6: S.tim.t6, t8: t }, vim, prog };
      if (process.env.LIVE_LOG && now - lastLog >= 1000) { lastLog = now; const l = Object.assign({}, doc); delete l.edges; fs.appendFileSync(process.env.LIVE_LOG, JSON.stringify(l) + "\n"); }
      fs.writeFileSync(json + ".tmp", JSON.stringify(doc)); fs.renameSync(json + ".tmp", json); }
  }
}
const ms = Date.now() - t0, edges = [];
for (let i = 1; i < rows.length; i++) if (rows[i][1] !== rows[i - 1][1]) edges.push(rows[i][0]);
console.log("FRL_LIVE samples", rows.length, "rate_hz", (rows.length / (ms / 1000)).toFixed(1), "pin_edges_ms", edges.join(" "));
console.log("FRL_LIVE timer", rows[0][2].toString(16), rows[rows.length - 1][2].toString(16));
fs.writeFileSync(out, rows.map(r => r.join(",")).join("\n") + "\n");
ds.shutdown();
