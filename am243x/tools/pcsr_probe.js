// Core load of R5FSS0-0 from the frl_load.h idle beacon in its TCMB (global 0x78107FF0), read through the system view with
// the core left running. The Cortex-R5 has no PC sampling register and the WFI status (MAIN_SEC_MMR PMSTAT) reads 0 from
// the debug port, so the program's own idle counter is the source. Prints the load each second.
// run.sh am243x/tools/pcsr_probe.js <seconds>
const { join } = require("path");
const secs = Number(process.argv[process.argv.length - 1]) || 10;
const ds = initScripting(); ds.setScriptingTimeout(30000);
const { nonDebugCores } = ds.configure(join(__dirname, "..", "AM243x_LP_" + (process.env.AM243X_SERIAL || "S24L0096") + ".ccxml"));
const d = ds.openSession(nonDebugCores.find(c => /CS_DAP_0/.test(c)));
d.target.connect();
const B = 0x78107FF0, r = a => Number(d.memory.readOne("0x" + a.toString(16) + "@System_View", 32)) >>> 0;
const read = () => ({ magic: r(B), t: r(B + 4), sleep: r(B + 8), wakes: r(B + 12), host: Date.now() });
let prev = read(); console.log("BEACON magic", prev.magic.toString(16));
const t0 = Date.now(); let reads = 0;
while (Date.now() - t0 < secs * 1000) {
  const s0 = Date.now(); while (Date.now() - s0 < 1000) { r(B + 8); reads++; }
  const cur = read(), dt = (cur.t - prev.t) >>> 0, ds_ = (cur.sleep - prev.sleep) >>> 0;
  console.log("SEC", Math.round((cur.host - t0) / 1000), "awake_pct", dt ? (100 * (1 - ds_ / dt)).toFixed(1) : "-", "chip_ms", (dt / 1000).toFixed(0), "wakes", (cur.wakes - prev.wakes) >>> 0);
  prev = cur;
}
console.log("RATE reads_per_s", (reads / secs).toFixed(0));
ds.shutdown();
