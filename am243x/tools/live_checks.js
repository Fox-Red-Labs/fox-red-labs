// Two checks of the LP-AM243 read straight from the debug port with the core running, one tight loop:
// 1. Share of time UART0's transmitter is busy, from its line status register.
// A program that sends B bytes a second at baud rate R keeps the transmitter busy 10*B/R of the time, so R = 10*B/duty: a check of the real baud
// rate that needs no divisor read (the divisor latch hides behind a write to the line control register, which this never does).
// Reading the line status register clears the receive error flags; the console receives nothing, so nothing is lost.
// 2. Whether GPIO0 pin 22 as the pad reads it (input data) follows the output register the program writes, one read after the other.
// 3. Whether each module's power-switch state (PSC) agrees with the module answering on the bus: an unpowered module reads 0 at its revision register.
// run.sh am243x/tools/live_checks.js <seconds> <out.txt>
const fs = require("fs"), { join } = require("path");
const out = process.argv[process.argv.length - 1], secs = Number(process.argv[process.argv.length - 2]);
const ds = initScripting(); ds.setScriptingTimeout(30000);
const { nonDebugCores } = ds.configure(join(__dirname, "..", "AM243x_LP_" + (process.env.AM243X_SERIAL || "S24L0096") + ".ccxml"));
const d = ds.openSession(nonDebugCores.find(c => /CS_DAP_0/.test(c)));
d.target.connect();
const rd = a => Number(d.memory.readOne("0x" + a.toString(16) + "@System_View", 32)) >>> 0;
const out2 = [];
for (const [name, mdstat, rev] of [["icssg0", 0x400878, 0x30026000], ["icssg1", 0x40087c, 0x300a6000], ["cpsw", 0x400884, 0x8000000], ["mcan0", 0x400830, 0x20701000], ["usb", 0x400818, 0xf900000], ["adc", 0x40081c, 0x28001000]]) {
  const st = rd(mdstat) & 0x3f, ans = rd(rev) !== 0; out2.push(`FRL_POWER ${name} psc_state ${st} answers ${ans ? 1 : 0} agree ${(st === 3) === ans ? 1 : 0}`); console.log(out2[out2.length - 1]); }
let n = 0, busy = 0, bursts = 0, inb = false, agree = 0, outHigh = 0, tPrev = 0, tFirst = 0, tLast = 0, tBefore = 0;
const dur = [];   // burst lengths in ms: the midpoint between the last idle and first busy read, to the midpoint between the last busy and next idle read
const t0 = Date.now();
while (Date.now() - t0 < secs * 1000) {
  const lsr = rd(0x2800014), tn = Date.now() - t0, tx = !(lsr & 0x40);   // bit 6 TEMT: 0 while the holding register or shifter has data
  const o = (rd(0x600014) >>> 22) & 1, i = (rd(0x600020) >>> 22) & 1;
  n++; if (tx) busy++; if (o === i) agree++; if (o) outHigh++;
  if (tx && !inb) { bursts++; tBefore = tPrev; tFirst = tn; }
  if (!tx && inb) dur.push(((tn + tLast) - (tBefore + tFirst)) / 2);
  if (tx) tLast = tn;
  inb = tx; tPrev = tn;
}
const ms = Date.now() - t0;
const ds_ = dur.slice().sort((a, b) => a - b), mean = dur.reduce((a, b) => a + b, 0) / (dur.length || 1);
const line = `FRL_CHECK samples ${n} busy ${busy} duty ${(busy / n).toFixed(6)} bursts ${bursts} burst_ms_mean ${mean.toFixed(3)} burst_ms_median ${ds_[ds_.length >> 1]} pad_follows_output ${agree} of ${n} output_high ${outHigh} secs ${(ms / 1000).toFixed(2)} loops_per_s ${(n / (ms / 1000)).toFixed(0)}`;
console.log(line); fs.writeFileSync(out, out2.join("\n") + "\n" + line + "\n");
ds.shutdown();
