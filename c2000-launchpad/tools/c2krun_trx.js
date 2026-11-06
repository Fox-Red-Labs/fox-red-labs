// Load the CC1407P host bench image (portability/cc1407p-host/c28x) into RAM, run it to the breakpoint at
// bench_done (or time out), and print what the driver did.
//   run.sh c2krun_trx.js <prog.out>
const { join } = require("path");
const prog = process.argv[process.argv.findIndex(x => x.endsWith(".out"))];
const ds = initScripting();
ds.setScriptingTimeout(60000);
ds.configure(join(__dirname, "F2800137_CL371081.ccxml"));
const s = ds.openSession(/C28xx_CPU1/);
s.target.connect();
s.target.reset();
s.memory.loadProgram(prog);
let stk = null;
try {
  const map = require("fs").readFileSync(prog.replace(/[^/]+\.out$/, "bench.map"), "utf8");
  const m = map.match(/^\.stack\s+\d+\s+([0-9a-f]{8})\s+([0-9a-f]{8})/m);
  if (m) { stk = [parseInt(m[1], 16), parseInt(m[2], 16)]; s.memory.fill(stk[0], stk[1], 0xA5A5, 16); }
} catch (e) { stk = null; }
s.breakpoints.add("bench_done");
const t0 = Date.now();
s.target.run();
console.log(`host_ms=${Date.now() - t0}`);
const rd = (sym, n) => s.memory.read(s.expressions.evaluate("&" + sym), n, 16).map(Number);
const hex = (a) => a.map(v => v.toString(16).toUpperCase().padStart(2, "0")).join(" ");
if (stk) {
  const w = s.memory.read(stk[0], stk[1], 16).map(Number);
  let top = w.length; while (top > 0 && w[top - 1] === 0xA5A5) top--;
  console.log(`stack_used_words=${top} of ${stk[1]}`);
}
console.log("sz " + rd("sz", 12).join(" "));
const c = rd("crc_check", 1)[0].toString(16).toUpperCase().padStart(4, "0");
console.log(`crc ${c} open ${rd("open_ok", 1)[0]} xfers ${rd("fake_xfers", 1)[0]} logn ${rd("fake_log_n", 1)[0]}`);
console.log("log " + hex(rd("fake_log", rd("fake_log_n", 1)[0])));
console.log("xfer_len " + rd("fake_xfer_len", 2).join(" "));
s.target.halt();
s.target.disconnect();
ds.shutdown();
