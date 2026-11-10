// Halt the four R5F cores and the M4F, then empty every hardware mailbox FIFO and clear its interrupt status, so a multi-core
// program (IPC notify) starts from a clean mailbox. RAM-only; run before loading the cores. Needed because a CPU reset leaves the mailbox as the last run left it.
const { join } = require("path");
const ds = initScripting();
ds.setScriptingTimeout(60000);
const { cores } = ds.configure(join(__dirname, "..", "AM243x_LP_" + (process.env.AM243X_SERIAL || "S24L0096") + ".ccxml"));
const sess = [];
for (const rx of [/Cortex_M4F_0/, /Cortex_R5_1_1/, /Cortex_R5_1_0/, /Cortex_R5_0_1/, /Cortex_R5_0_0/]) {
  const s = ds.openSession(cores.find(c => rx.test(c)));
  s.target.connect();
  try { s.target.halt(); } catch (e) {}
  sess.push(s);
}
const s = sess[sess.length - 1];
let drained = 0;
for (let n = 0; n < 8; n++) {
  const base = 0x29000000 + n * 0x10000;
  try {
    for (let f = 0; f < 16; f++) {
      let guard = 0;
      while ((Number(s.memory.readOne(base + 0xC0 + 4 * f)) & 7) > 0 && guard++ < 16) { s.memory.readOne(base + 0x40 + 4 * f); drained++; }
    }
    for (let u = 0; u < 4; u++) s.memory.write(base + 0x104 + 0x10 * u, [0xffffffff], 32);
  } catch (e) { console.log("FRL_MBOX cluster", n, "not accessible"); }
}
console.log("FRL_MBOX drained", drained);
ds.shutdown();
